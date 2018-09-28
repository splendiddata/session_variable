/*
 * Copyright (c) Splendid Data Product Development B.V. 2013
 *
 * This program is free software: You may redistribute and/or modify under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at Client's option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, Client should obtain one via www.gnu.org/licenses/.
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "commands/dbcommands.h"
#include "storage/fd.h"
#include <sys/stat.h>
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"
#include "parser/parse_coerce.h"
#include "utils/array.h"

#include "session_variable.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC
;
#endif

static bool virgin = true;
static SessionVariable* variables = NULL;
static bool pgInitInvoked = false;
static bool isExecutingInitialisationFunction = false;

/*
 * function prototypes
 */
void buildBTree(void);
bool checkTypeType(Oid typeOid);
Datum coerceInput(Oid inputType, Oid internalType, int internalTypeLength,
		Datum input, bool* castFailed);
Datum coerceOutput(Oid internalType, int internalTypeLength, Datum internalData,
		Oid outputType, bool* castFailed);
SessionVariable* createVariable(text* variableName, bool isConst, Oid valueType,
		int typeLength, bool isNull, Datum value);
void deleteVariable(text* variablename);
Datum deserializeV1(text* varName, Oid dataType, Datum detoastedValue);
Datum deserializeV2(text* varName, Oid dataType, Datum detoastedValue);
int getTypeLength(Oid typeOid);
bool insertVariable(SessionVariable* variable);
void invokeInitialisationFunction(void);
void logVariable(int logLevel, char* leadingText, SessionVariable* variable);
int reload(void);
void removeVariableRecursively(SessionVariable* v);
bool saveNewVariable(text* variableName, bool isConst, Oid valueType,
		int typeLength, bool isNull, Datum value);
SessionVariable* searchVariable(char* variableName, SessionVariable** lvl,
		bool* found);
Datum serializeV1(SessionVariable* variable);
Datum serializeV2(SessionVariable* variable);
void updateRecursively(SessionVariable* var);
void updateVariable(SessionVariable* variable);

/*
 * Some fields to support both version 1 and version 2
 */
Datum (*deserialize)(text* varName, Oid dataType,
		Datum detoastedValue) = &deserializeV2;
Datum (*serialize)(SessionVariable* variable) = &serializeV2;
Oid initialValueTypeOid = TEXTOID;
bool version1 = false;

void _PG_init(void);
void _PG_init()
{
	char* sql =
			"select extversion from pg_extension where extname = 'session_variable'";
	Portal cursor;
	char* installedVersion;

	if (IsBackgroundWorker || pgInitInvoked)
	{
		return;
	}
	pgInitInvoked = true;

	/*
	 * Read the session_variable.variables table and update each row to
	 * version 2 format
	 */
	SPI_connect();
	cursor = SPI_cursor_open_with_args(NULL, sql, 0, NULL, NULL, NULL, true,
	CURSOR_OPT_BINARY | CURSOR_OPT_NO_SCROLL);
	SPI_cursor_fetch(cursor, true, 1);
	installedVersion = SPI_getvalue(SPI_tuptable->vals[0],
			SPI_tuptable->tupdesc, 1);
	SPI_cursor_close(cursor);
	SPI_finish();

	if (!strcmp(installedVersion, "1.0"))
	{
		deserialize = &deserializeV1;
		serialize = &serializeV1;
		initialValueTypeOid = BYTEAOID;
		version1 = true;
	}
	else
	{
		deserialize = &deserializeV2;
		serialize = &serializeV2;
		initialValueTypeOid = TEXTOID;
	}
}
/*
 * Finds the type length in the type cache. -1 for varlena
 *
 * @param Oid typeOid - Identification of the type
 * @return int typlen from pg_type
 */
int getTypeLength(Oid typeOid)
{
	HeapTuple typTup;
	Form_pg_type typ;
	int result;

	typTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeOid));
	if (!HeapTupleIsValid(typTup))
	{
		elog(ERROR, "cache lookup failed for type %u", typeOid);
		return false;
	}
	typ = (Form_pg_type) GETSTRUCT(typTup);
	result = typ->typlen;
	ReleaseSysCache(typTup);

	return result;
}

/*
 * Checks if the type identified by the typeOid is not a pseudo type.
 * If the type happens to be a pseudo type then an error is logged.
 *
 * @param Oid typeOid - identification of the type
 * @return bool - true if ok, false when an error message has been logged
 */
bool checkTypeType(Oid typeOid)
{
	HeapTuple typTup;
	Form_pg_type typ;
	bool result = true;

	typTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeOid));
	if (!HeapTupleIsValid(typTup))
	{
		elog(ERROR, "cache lookup failed for type %u", typeOid);
		return false;
	}
	typ = (Form_pg_type) GETSTRUCT(typTup);
	result = typ->typtype != TYPTYPE_PSEUDO;
	ReleaseSysCache(typTup);

	if (!result)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED), (errmsg("pseudo types are not supported: %s", getTypeName(typeOid) ))));
	}
	return result;

}

/*
 * Returns a Datum in the expected type or null if no (assignment) implicit cast can be found.
 *
 * BEWARE! the output is malloced instead of palloced. Thus it can be stored directly as session variable.
 *
 * @param Oid inputType: The data type of the input data as obtained from the function invocation
 * @param Oid expectedType: The data type that is used for internal storage of the session variable
 * @param int typeLength: -1 for varlena or the number of bytes for scalars
 * @param Datum input: The (detoasted) input
 * @param bool* castFailed: will be set to false if the coercion completed correctly of true if it failed
 * @return Datum
 */
Datum coerceInput(Oid inputType, Oid internalType, int internalTypeLength,
		Datum input, bool* castFailed)
{
	CoercionPathType coercionPathType;
	Oid coercionFunctionOid;
	Oid outputFunctionOid;
	Oid inputFunctionOid;
	Oid inputFunctionParam;
	Datum coercedInput;
	Datum mallocedResult;
	char* stringValue;
	bool typeIsVarlena;

	*castFailed = true;

	if (inputType == internalType)
	{
		coercionPathType = COERCION_PATH_RELABELTYPE;
	}
	else
	{
		coercionPathType = find_coercion_pathway(internalType, inputType,
				COERCION_EXPLICIT, &coercionFunctionOid);
	}

	switch (coercionPathType)
	{
	case COERCION_PATH_RELABELTYPE:
		coercedInput = input;
		break;
	case COERCION_PATH_FUNC:
		coercedInput = OidFunctionCall1(coercionFunctionOid, input);
		break;
	case COERCION_PATH_COERCEVIAIO:
		getTypeOutputInfo(inputType, &outputFunctionOid, &typeIsVarlena);
		stringValue = OidOutputFunctionCall(outputFunctionOid, input);
		getTypeInputInfo(internalType, &inputFunctionOid, &inputFunctionParam);
		coercedInput = OidInputFunctionCall(inputFunctionOid, stringValue,
				inputFunctionParam, -1);
		break;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("value must be of type %s, but is of type %s", getTypeName(inputType) ,getTypeName(internalType) ))));
		return (Datum) NULL;
		break;
	}

	if (internalTypeLength < 0)
	{
		mallocedResult = (Datum) malloc(VARSIZE(coercedInput));
		SET_VARSIZE(mallocedResult, VARSIZE(coercedInput));
		memcpy(VARDATA(mallocedResult), VARDATA(coercedInput),
		VARSIZE(coercedInput) - VARHDRSZ);
	}
	else if (internalTypeLength > SIZEOF_DATUM)
	{
		mallocedResult = (Datum) malloc(internalTypeLength);
		memcpy((void*) mallocedResult, (void*) coercedInput,
				internalTypeLength);
	}
	else
	{
		mallocedResult = coercedInput;
	}

	*castFailed = false;

	return mallocedResult;
}

/*
 * Returns a Datum in the expected type or null if no (assignment) implicit cast can be found. The result is palloced if necessary
 *
 * @param Oid internalType: The defined type of the session variable
 * @param int internalTypeLength: The typlen of the defined type of the session variable
 * @param Datum internalData: The content of the variable
 * @param Oid outputType: The type of the expected output
 * @param bool* castFailed: will be set to false if the coercion completed correctly of true if it failed
 * @return Datum
 */
Datum coerceOutput(Oid internalType, int internalTypeLength, Datum internalData,
		Oid outputType, bool* castFailed)
{
	CoercionPathType coercionPathType;
	Oid coercionFunctionOid;
	Oid outputFunctionOid;
	Oid inputFunctionOid;
	Oid inputFunctionParam;
	Datum result;
	char* stringValue;
	bool typeIsVarlena;

	*castFailed = true;

	if (internalType == outputType)
	{
		coercionPathType = COERCION_PATH_RELABELTYPE;
	}
	else
	{
		coercionPathType = find_coercion_pathway(outputType, internalType,
				COERCION_EXPLICIT, &coercionFunctionOid);
	}

	switch (coercionPathType)
	{
	case COERCION_PATH_RELABELTYPE:
		if (internalTypeLength < 0)
		{
			result = (Datum) palloc(VARSIZE(internalData));
			SET_VARSIZE(result, VARSIZE(internalData));
			memcpy(VARDATA(result), VARDATA(internalData),
			VARSIZE(internalData) - VARHDRSZ);

		}
		else if (internalTypeLength > SIZEOF_DATUM)
		{
			result = (Datum) palloc(internalTypeLength);
			memcpy((void*) result, &internalData, internalTypeLength);
		}
		else
		{
			result = internalData;
		}
		*castFailed = false;
		return result;
	case COERCION_PATH_FUNC:
		result = OidFunctionCall1(coercionFunctionOid, internalData);
		*castFailed = false;
		return result;
	case COERCION_PATH_COERCEVIAIO:
		getTypeOutputInfo(internalType, &outputFunctionOid, &typeIsVarlena);
		stringValue = OidOutputFunctionCall(outputFunctionOid, internalData);
		getTypeInputInfo(outputType, &inputFunctionOid, &inputFunctionParam);
		result = OidInputFunctionCall(inputFunctionOid, stringValue,
				inputFunctionParam, -1);
		*castFailed = false;
		return result;
	default:
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("The variable's internal type %s, cannot be cast to type %s", getTypeName(internalType) ,getTypeName(outputType) ))));
		return (Datum) NULL;
	}
}

/*
 * Writes the peculiarities of one variable to the log for debugging purposes
 *
 * @param int logLevel The log level
 * @param char* leadingText Some text to indicate why the variable is logged
 * @param SessionVariable* variable The variable to log
 */
void logVariable(int logLevel, char* leadingText, SessionVariable* variable)
{
	if (variable == NULL)
	{
		elog(logLevel, "%s 0:null", leadingText);
	}
	elog(logLevel,
			"%s %p:%s, type=%d,%s, typeLength=%d, isConstant=%d, isNull=%d, prior=%p:%s, next=%p:%s",
			leadingText, variable, variable->name, variable->type,
			getTypeName(variable->type), variable->typeLength,
			variable->isConstant, variable->isNull, variable->prior,
			variable->prior == NULL ? "-" : variable->prior->name,
			variable->next,
			variable->next == NULL ? "-" : variable->next->name);
}

/*
 * Recursively removes the variable definition and all its subordinates
 *
 * @param SessionVariable* v The variable definition to be removed. If null, the function will return immediately.
 */
void removeVariableRecursively(SessionVariable* v)
{
	if (v == NULL)
	{
		return;
	}

	logVariable(DEBUG3, "remove:", v);

	removeVariableRecursively(v->prior);
	removeVariableRecursively(v->next);

	/*
	 * The variable is created with malloc() instead of palloc(), so must be
	 * freed using free() here instead of pfree().
	 */
	if (v->typeLength < 0 || v->typeLength > SIZEOF_DATUM)
	{
		free((void*) v->content);
	}
	free((void*) v->name);
	free((void*) v);
}

/*
 * Creates the variable
 *
 * @param text* variableName Name of the variable
 * @param bool isConst Is the variable a constant
 * @param Oid valueType the type of the content
 * @param int typeLength  The length of the type, -1 for varlena
 * @param Datum value or null if none. BEWARE! The content of value is supposed to be malloced instead of palloced!
 */
SessionVariable* createVariable(text* variableName, bool isConst, Oid valueType,
		int typeLength, bool isNull, Datum value)
{
	/*
	 * The variable is to be stored as session variable (read: static variable).
	 * So we do not use palloc() here, but malloc() instead.
	 */
	SessionVariable* result = (SessionVariable*) malloc(
			sizeof(SessionVariable));

	elog(
			DEBUG3,
			"createVariable(%s, isConst=%d, valueType=%d, typeLength=%d, isNull=%d, value)",
			text_to_cstring(variableName), isConst, valueType, typeLength,
			isNull);

	result->prior = NULL;
	result->next = NULL;

	result->name = (char*) malloc(VARSIZE(variableName) - VARHDRSZ + 1);
	memcpy(result->name, VARDATA(variableName),
	VARSIZE(variableName) - VARHDRSZ);
	result->name[VARSIZE(variableName) - VARHDRSZ] = '\0';
	result->isConstant = isConst;
	result->type = valueType;
	result->typeLength = typeLength;
	result->isNull = isNull;
	result->content = value;

	logVariable(DEBUG2, "createVariable() = ", result);
	return result;
}

/*
 * Transforms the list of SessionVariables that has just been loaded from the session_variable.variables table
 * into a binary tree.
 * It expects variables static variable to contain the head of an ordered list of SessionVariables, with each
 * next pointer containing the address of the next SessionVariable or null at the end of the list.
 * The variables static variable will be overwritten with the starting point of the b-tree.
 */
void buildBTree(void)
{
	SessionVariable *btreeHelper[32];
	SessionVariable *lower, *curr, *next;
	int i;

	for (i = 0; i < 32; i++)
	{
		btreeHelper[i] = NULL;
	}

	curr = variables;
	while (curr != NULL)
	{
		next = curr->next;
		curr->next = NULL;
		lower = NULL;
		for (i = 0; btreeHelper[i] != NULL; i++)
		{
			btreeHelper[i]->next = lower;
			lower = btreeHelper[i];
			btreeHelper[i] = NULL;
		}
		btreeHelper[i] = curr;
		curr->prior = lower;
		curr = next;
	}

	variables = NULL;
	for (i = 0; i < 32; i++)
	{
		if (btreeHelper[i] != NULL)
		{
			if (variables == NULL)
			{
				variables = btreeHelper[i];
			}
			else
			{
				btreeHelper[i]->next = variables;
				variables = btreeHelper[i];
			}
		}
	}
}

Datum deserializeV1(text* varName, Oid dataType, Datum detoastedValue)
{
	Datum value;
	int typeLength = getTypeLength(dataType);

	value = (Datum) VARDATA(detoastedValue);

	if (typeLength > 0 && typeLength <= SIZEOF_DATUM)
	{
		/*
		 * For fixed length types of 8 bytes or shorter, the VARDATA is the
		 * Datum we are looking for - not a reference. So take it as value.
		 */
		Datum tmp;
		memcpy(&tmp, (void*) value, SIZEOF_DATUM);
		value = tmp;
	}

	if ((typeLength < 0 && VARSIZE(detoastedValue) - VARHDRSZ != VARSIZE(value))
			|| (typeLength > 0 && typeLength <= SIZEOF_DATUM
					&& VARSIZE(detoastedValue) - VARHDRSZ != SIZEOF_DATUM)
			|| (typeLength > SIZEOF_DATUM
					&& VARSIZE(detoastedValue) - VARHDRSZ != typeLength))
	{
		/*
		 * The Datum containing the content of the value is wrapped in a bytea datum and stored.
		 * So the size of the content must match the content size of the wrapping bytea (= VARSIZE - VARHDRSZ).
		 * If this is not the case, someone has manually altered the content.
		 */
		elog(
				LOG,
				"Someone has been messing with variable '%s', expected length=%d, actual length = %d",
				text_to_cstring(varName),
				typeLength < 0 ? VARSIZE(value) : typeLength < SIZEOF_DATUM ? SIZEOF_DATUM : typeLength,
				VARSIZE(detoastedValue) - VARHDRSZ);
		elog(
				NOTICE,
				"Session variable '%s' is incorrectly stored in the session_variable.variables table",
				text_to_cstring(varName));
		return (Datum) NULL;
	}

	return value;
}

Datum deserializeV2(text* varName, Oid dataType, Datum detoastedValue)
{
	HeapTuple typTup;
	Form_pg_type pgTyp;
	Oid typInputFunctionOid;
	Oid typIoParam;
	char* valueCString;
	Datum result;

	/*
	 * Get the type specs
	 */
	typTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(dataType));
	if (!HeapTupleIsValid(typTup))
	{
		elog(ERROR, "cache lookup failed for type %u", dataType);
		return (Datum) NULL;
	}
	pgTyp = (Form_pg_type) GETSTRUCT(typTup);
	typInputFunctionOid = pgTyp->typinput;
	typIoParam = getTypeIOParam(typTup);
	ReleaseSysCache(typTup);

	/*
	 * Invoke the typinput function
	 */
	valueCString = text_to_cstring((text*) detoastedValue);
	result = OidInputFunctionCall(typInputFunctionOid, valueCString, typIoParam,
			-1);
	pfree(valueCString);
	return result;
}

/*
 * Walks through the session_variable.variables table to build the SessionVariable b-tree in variables.
 * ----------------------------------------------------------------------------
 * A _PG_INIT function appears not usable for this purpose because access
 * rights on the session_variable.variables table appear to be handled
 * differently when invoked as part of the initialisation from a preload
 * library or when invoked in "normal" code processing. So every function that
 * is externally accessible checks if 'virgin' is still set and if so invokes
 * reload(). This function resets 'virgin'.
 * ----------------------------------------------------------------------------
 *
 * @return int The number or SessionVariables created
 */
int reload()
{
	char* sql = "select variable_name"
			", is_constant"
			", typ.oid"
			", initial_value"
			" from session_variable.variables var"
			" join pg_catalog.pg_namespace nsp"
			" on var.variable_type_namespace = nsp.nspname"
			" join pg_catalog.pg_type typ"
			" on nsp.oid = typ.typnamespace"
			" and var.variable_type_name = typ.typname"
			" and typ.typtype <> 'p'"
			" order by variable_name";
	text* variableName = NULL;
	bool isConstValue = false;
	Oid valueType = InvalidOid;
	Datum value = (Datum) NULL;
	Datum mallocedValue;
	bool isNull;
	Portal cursor = NULL;
	int nrVariables = 0;
	SessionVariable** nextVar = &variables;
	int typeLength;
	bool castFailed;
	Datum rawValue;
	Datum detoastedValue;

	/*
	 * Clear the old content (if any).
	 */
	removeVariableRecursively(variables);
	variables = NULL;
	virgin = false;

	elog(DEBUG3, "execute query: %s", sql);

	SPI_connect();

	/*
	 * Walk through the session_variable.variables table
	 */
	cursor = SPI_cursor_open_with_args(NULL, sql, 0, NULL, NULL, NULL, true,
	CURSOR_OPT_BINARY | CURSOR_OPT_NO_SCROLL);
	SPI_cursor_fetch(cursor, true, 1);
	while (!cursor->atEnd)
	{
		variableName = (text*) PG_DETOAST_DATUM(
				SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1,
						&isNull));
		isConstValue = 't'
				== *SPI_getvalue(SPI_tuptable->vals[0], SPI_tuptable->tupdesc,
						2);
		valueType = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc,
				3, &isNull);
		typeLength = getTypeLength(valueType);
		rawValue = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc,
				4, &isNull);
		if (isNull)
		{
			mallocedValue = (Datum) NULL;
		}
		else
		{
			detoastedValue = (Datum) PG_DETOAST_DATUM(rawValue);

			value = deserialize(variableName, valueType, detoastedValue);

			/*
			 * we want the value to be malloced instead of palloced.
			 */
			mallocedValue = coerceInput(valueType, valueType, typeLength, value,
					&castFailed);

			isNull = false;
		}

		/*
		 * Create the new value and link it to its predecessor.
		 */
		*nextVar = createVariable(variableName, isConstValue, valueType,
				typeLength, isNull, mallocedValue);
		nextVar = &(*nextVar)->next;

		nrVariables++;
		SPI_cursor_fetch(cursor, true, 1);
	}
	SPI_cursor_close(cursor);
	SPI_finish();

	buildBTree();

	invokeInitialisationFunction();

	elog(DEBUG3, "reload() = %d", nrVariables);
	return nrVariables;
}

/*
 * Checks if a no-argument function called session_variable.initialze_variables()
 * exists and, if so, invokes it.
 */
void invokeInitialisationFunction()
{
	Oid namespaceOid;
	char* dbName;

	dbName = get_database_name(MyDatabaseId);
	namespaceOid = get_namespace_oid("session_variable", true);
	if (OidIsValid(namespaceOid))
	{
		/*
		 * Check if a no-argument function called "session_variable_initialisation" exists in namespace
		 * "session_variable"
		 */
		oidvector* emptyArgVector = buildoidvector(NULL, 0);
		if (SearchSysCacheExists3(PROCNAMEARGSNSP,
				CStringGetDatum("variable_initialisation"),
				PointerGetDatum(buildoidvector(NULL, 0)),
				ObjectIdGetDatum(namespaceOid)))
		{

			SPI_connect();

			elog(
					DEBUG3,
					"login_hook will execute select session_variable.variable_initialisation() in database %s",
					dbName);
			isExecutingInitialisationFunction = true;
			SPI_execute("select session_variable.variable_initialisation()",
			false, 1);
			isExecutingInitialisationFunction = false;
			elog(
					DEBUG3,
					"login_hook is back from select session_variable.variable_initialisation() in database %s",
					dbName);

			SPI_finish();
		}
		else
		{
			elog(
					DEBUG1,
					"Function session_variable.variable_initialisation() is not invoked because it does not exist in database %s",
					dbName);
		}

		pfree(emptyArgVector);
	}
}

/*
 * function session_variable.is_executing_variable_initialisation() returns boolean.
 *
 * This function returns true if the session_variable.variable_initialisation() function is executing
 * under control of the session_variable code.
 */
PG_FUNCTION_INFO_V1(is_executing_variable_initialisation);
Datum is_executing_variable_initialisation( PG_FUNCTION_ARGS)
{
	PG_RETURN_BOOL(isExecutingInitialisationFunction);
}

/*
 * Serializes the memory image of the variable's content into a bytea
 *
 * @param SessionVariable* variable - the variable to serialize
 * @return Datum - The content wrapped in a bytea
 */
Datum serializeV1(SessionVariable* variable)
{
	Datum contentWrapper;

	if (variable->isNull)
	{
		return (Datum) NULL;
	}

	if (variable->typeLength < 0)
	{
		contentWrapper = (Datum) palloc(
		VARHDRSZ + VARSIZE(variable->content));
		SET_VARSIZE(contentWrapper, VARHDRSZ + VARSIZE(variable->content));
		memcpy(VARDATA(contentWrapper), (void*) variable->content,
				VARSIZE(variable->content));
	}
	else if (variable->typeLength > SIZEOF_DATUM)
	{
		contentWrapper = (Datum) palloc(VARHDRSZ + variable->typeLength);
		SET_VARSIZE(contentWrapper, VARHDRSZ + variable->typeLength);
		memcpy(VARDATA(contentWrapper), (void*) variable->content,
				variable->typeLength);
	}
	else
	{
		contentWrapper = (Datum) palloc(VARHDRSZ + SIZEOF_DATUM);
		SET_VARSIZE(contentWrapper, VARHDRSZ + SIZEOF_DATUM);
		memcpy(VARDATA(contentWrapper), &variable->content,
		SIZEOF_DATUM);
	}
	return contentWrapper;
}

/*
 * Converts the content of the variable to a text Datum using the type's
 * outfunction.
 *
 * @param SessionVariable* variable - The variable of which the content is to be
 *                                    serialized
 * @return Datum - The initial value serialized using the type's outfunction
 *                 or null if the variable's content was null
 */
Datum serializeV2(SessionVariable* variable)
{
	HeapTuple typTup;
	Form_pg_type typ;
	Oid typOutputFunctionOid;
	char* serialized;
	Datum result;

	if (variable->isNull)
	{
		return (Datum) NULL;
	}

	/*
	 * Get the type specs
	 */
	typTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(variable->type));
	if (!HeapTupleIsValid(typTup))
	{
		elog(ERROR, "cache lookup failed for type %u", variable->type);
		return (Datum) NULL;
	}
	typ = (Form_pg_type) GETSTRUCT(typTup);
	typOutputFunctionOid = typ->typoutput;
	ReleaseSysCache(typTup);

	/*
	 * Serialize the initialValue into a character array
	 */
	serialized = OidOutputFunctionCall(typOutputFunctionOid, variable->content);
	if (!serialized)
	{
		/*
		 * something went wrong. Let's hope it is logged
		 */
		return (Datum) NULL;
	}

	/*
	 * Wrap into a bytea
	 */
	result = (Datum) cstring_to_text(serialized);
	pfree(serialized);

	return result;
}

/*
 * Inserts the variable into the session_variable.variables table
 *
 * @param SessionVariable* variable
 * @return bool: true if ok
 */
bool insertVariable(SessionVariable* variable)
{
	char* sql = "insert into session_variable.variables "
			"( variable_name"
			", is_constant"
			", variable_type_namespace"
			", variable_type_name"
			", initial_value"
			") "
			"select $1"
			", $2"
			", nsp.nspname"
			", typ.typname"
			", $4 "
			"from pg_catalog.pg_type typ "
			"join pg_catalog.pg_namespace nsp on typ.typnamespace = nsp.oid "
			"where typ.oid = $3";
	int32 nrArgs = 4;
	Oid oid[nrArgs];
	Datum val[nrArgs];
	char nulls[nrArgs];
	bool result;

	oid[0] = TEXTOID;
	val[0] = (Datum) cstring_to_text(variable->name);
	nulls[0] = ' ';
	oid[1] = BOOLOID;
	val[1] = BoolGetDatum(variable->isConstant);
	nulls[1] = ' ';
	oid[2] = REGTYPEOID;
	val[2] = ObjectIdGetDatum(variable->type);
	nulls[2] = ' ';
	oid[3] = initialValueTypeOid;
	if (variable->isNull)
	{
		val[3] = PointerGetDatum(NULL);
		nulls[3] = 'n';
	}
	else
	{
		val[3] = serialize(variable);
		nulls[3] = ' ';
	}

	SPI_connect();
	result = SPI_execute_with_args(sql, nrArgs, oid, val, nulls, false, 1);
	SPI_finish();

	return result;
}

/*
 * Updates the variable's initial value in the session_variable.variables table
 *
 * @param SessionVariable* variable
 */
void updateVariable(SessionVariable* variable)
{
	char* sql = "update session_variable.variables"
			" set initial_value = $1"
			" where variable_name = $2";
	int32 nrArgs = 2;
	Oid oid[nrArgs];
	Datum val[nrArgs];
	char nulls[nrArgs];

	oid[0] = initialValueTypeOid;
	if (variable->isNull)
	{
		val[0] = PointerGetDatum(NULL);
		nulls[0] = 'n';
	}
	else
	{
		val[0] = serialize(variable);
		nulls[0] = ' ';
	}

	oid[1] = TEXTOID;
	val[1] = (Datum) cstring_to_text(variable->name);
	nulls[1] = ' ';

	SPI_connect();
	SPI_execute_with_args(sql, nrArgs, oid, val, nulls, false, 1);
	SPI_finish();
}

/*
 * Deletes the directory reference from the file_system.directory_reference table
 *
 * @param text* directoryRefereceName
 * @return int the result of the SPI_execute_with_args() invocation
 */
void deleteVariable(text* variableName)
{
	char* sql =
			"delete from session_variable.variables where variable_name = $1";
	int32 nrArgs = 1;
	Oid oid[nrArgs];
	Datum val[nrArgs];
	oid[0] = TEXTOID;
	val[0] = (Datum) variableName;

	SPI_connect();
	SPI_execute_with_args(sql, nrArgs, oid, val, NULL, false, 1);
	SPI_finish();
}

/*
 * Searches the binary tree for the variableName
 *
 * If a matching name is found, found wil be set to true and the var with the corresponding name will
 * be returned.
 *
 * If no variables exist, NULL will be returned.
 *
 * Else found will be set to false and the variable under which the new variable is to be positioned
 * will be returned. So if the variableName is less than the name in the returned variable, then the
 * prior pointer of the returned variable will be NULL. Otherwise the next pointer of the returned
 * variable will be NULL.
 *
 * @param char* variableName: Name to be found
 * @param SessionVariable** lvl: The variables static variable or a parent level under which to search
 * @param bool* found: Will be set true if the variableName is found or false if not
 * @return SessionVariable*: If found == true, the var that we were looking for.
 *              else the var under which the new variable should be stored of null if no variables exist yet.
 */
SessionVariable* searchVariable(char* variableName, SessionVariable** lvl,
		bool* found)
{
	int diff;

	elog(DEBUG2, "searchVariable('%s')", variableName);

	if (*lvl == NULL)
	{
		*found = false;
		elog(DEBUG2, "searchVariable(%s) = not found", variableName);
		return NULL;
	}

	diff = strcmp(variableName, (*lvl)->name);

	if (diff < 0)
	{
		if ((*lvl)->prior == NULL)
		{
			*found = false;
			return *lvl;
		}
		logVariable(DEBUG2, "not found (yet) search(prior):", (*lvl));
		return searchVariable(variableName, &(*lvl)->prior, found);
	}

	if (diff > 0)
	{
		if ((*lvl)->next == NULL)
		{
			*found = false;
			return *lvl;
		}
		logVariable(DEBUG2, "not found (yet) search(next):", (*lvl));
		return searchVariable(variableName, &(*lvl)->next, found);
	}

	*found = true;
	logVariable(DEBUG2, "searchVariable() = ", *lvl);
	return *lvl;
}

/*
 * Stores the variable with the specified data in memory and in the session_variable.variables table
 *
 * @param text* variableName: Name of the variable to be stored
 * @param bool isConst: Is this variable to be treated as constant
 * @param Oid valueType: Data type of the variable content
 * @param int typeLength: Is this a variable length data type
 * @param bool isNull: Is the content NULL
 * @param Datum value: The value of the variable, may be NULL
 * @return bool: true if ok
 */
bool saveNewVariable(text* variableName, bool isConst, Oid valueType,
		int typeLength, bool isNull, Datum value)
{
	bool found;
	SessionVariable* parentLevel;
	SessionVariable* variable;
	char* varName = text_to_cstring((text*) variableName);

	parentLevel = searchVariable(varName, &variables, &found);
	if (found)
	{
		if (value && typeLength < 0)
		{
			/*
			 * The value has been malloced instead of palloced, so must be freed if an error occurs.
			 */
			free((void*) value);
		}
		if (value && typeLength > SIZEOF_DATUM)
		{
			free((void*) value);
		}
		ereport(ERROR,
				(errcode(ERRCODE_UNIQUE_VIOLATION) , (errmsg("Variable \"%s\" already exists", varName ))));
		return false;
	}

	variable = createVariable(variableName, isConst, valueType, typeLength,
			isNull, value);

	if (parentLevel == NULL)
	{
		variables = variable;
	}
	else
	{
		int diff = strcmp(varName, parentLevel->name);

		if (diff < 0)
		{
			parentLevel->prior = variable;
		}
		else
		{
			parentLevel->next = variable;
		}
	}

	return insertVariable(variable);
}

/*
 * create_variable(variable_name text, variable_type regtype) returns boolean
 * create_variable(variable_name text, variable_type regtype, initial_value anyelement) returns boolean
 */
PG_FUNCTION_INFO_V1(create_variable);
Datum create_variable( PG_FUNCTION_ARGS)
{
	text* variableName = NULL;
	Oid type = InvalidOid;
	Oid typeOid = InvalidOid;
	Datum content = (Datum) NULL;
	Oid contentTypeOid = InvalidOid;
	int typeLength;
	bool isNull = false;
	bool result;
	int contentTypeLength;
	bool castFailed;
	char* variableNameStr;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() < 2 || PG_NARGS() > 3)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.create_variable(variable_name text, variable_type regtype) " "or session_variable.create_variable(variable_name text, variable_type regtype, initial_value anyelement)"))));
		PG_RETURN_BOOL(false);
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable name must be filled"))));
		PG_RETURN_BOOL(false);
	}
	variableName = PG_GETARG_TEXT_P(0);

	if (PG_ARGISNULL(1))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable type must be filled"))));
		PG_RETURN_BOOL(false);
	}
	type = PG_GETARG_OID(1);
	typeOid = DatumGetObjectId(type);
	if (!checkTypeType(typeOid))
	{
		PG_RETURN_BOOL(false);
	}
	typeLength = getTypeLength(typeOid);

	variableNameStr = text_to_cstring(variableName);
	elog(DEBUG1, "@>create_variable('%s')", variableNameStr);
	if (!strlen(variableNameStr))
	{
		ereport(ERROR,
				(errcode(ERRCODE_ZERO_LENGTH_CHARACTER_STRING), (errmsg("variable name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	isNull = PG_NARGS() < 3 || PG_ARGISNULL(2);
	if (!isNull)
	{
		contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 2);
		if (typeOid == contentTypeOid)
		{
			contentTypeLength = typeLength;
		}
		else
		{
			contentTypeLength = getTypeLength(contentTypeOid);
		}
		if (contentTypeLength < 0)
		{
			content = (Datum) PG_GETARG_VARLENA_P(2);
		}
		else
		{
			content = PG_GETARG_DATUM(2);
		}

		/*
		 * Make sure the new content is malloced instead of palloced, and cast to the right type of course.
		 */
		content = coerceInput(contentTypeOid, typeOid, typeLength, content,
				&castFailed);
		if (castFailed)
		{
			/*
			 * Something went wrong, but that has already been logged
			 */
			PG_RETURN_BOOL(false);
		}
	}

	result = saveNewVariable(variableName, false, type, typeLength, isNull,
			content);

	elog(DEBUG1, "@<create_variable('%s')", variableNameStr);

	PG_RETURN_BOOL(result);
}

/*
 * create_constant(constant_name text, constant_type regtype, value anyelement) returns boolean
 */
PG_FUNCTION_INFO_V1(create_constant);
Datum create_constant( PG_FUNCTION_ARGS)
{
	text* variableName = NULL;
	Oid type = InvalidOid;
	Oid typeOid = InvalidOid;
	Datum content = (Datum) NULL;
	Oid contentTypeOid = InvalidOid;
	int typeLength;
	int contentTypeLength;
	bool result;
	bool castFailed;
	char* variableNameStr;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 3)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.create_constant(constant_name text, constant_type regtype, value anyelement)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("constant name must be filled"))));
		PG_RETURN_BOOL(false);
	}
	variableName = PG_GETARG_TEXT_P(0);

	if (PG_ARGISNULL(1))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("constant type must be filled"))));
		PG_RETURN_BOOL(false);
	}
	type = PG_GETARG_OID(1);
	typeOid = DatumGetObjectId(type);
	if (!checkTypeType(typeOid))
	{
		PG_RETURN_BOOL(false);
	}
	typeLength = getTypeLength(typeOid);

	variableNameStr = text_to_cstring(variableName);
	elog(DEBUG1, "@>create_constant('%s')", variableNameStr);
	if (!strlen(variableNameStr))
	{
		ereport(ERROR,
				(errcode(ERRCODE_ZERO_LENGTH_CHARACTER_STRING), (errmsg("constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	if (!PG_ARGISNULL(2))
	{
		contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 2);
		if (typeOid == contentTypeOid)
		{
			contentTypeLength = typeLength;
		}
		else
		{
			contentTypeLength = getTypeLength(contentTypeOid);
		}
		if (contentTypeLength < 0)
		{
			content = (Datum) PG_GETARG_VARLENA_P(2);
		}
		else
		{
			content = PG_GETARG_DATUM(2);
		}

		/*
		 * Make sure the new content is malloced instead of palloced, and cast to the right type of course.
		 */
		content = coerceInput(contentTypeOid, typeOid, typeLength, content,
				&castFailed);
		if (castFailed)
		{
			/*
			 * Something went wrong, but that has already been logged
			 */
			PG_RETURN_BOOL(false);
		}
	}

	result = saveNewVariable(variableName, true, type, typeLength,
			PG_ARGISNULL(2), content);

	elog(DEBUG1, "@<create_constant('%s')", variableNameStr);

	PG_RETURN_BOOL(result);
}

/*
 * drop(variable_constant_name text) returns boolean true
 */
PG_FUNCTION_INFO_V1(drop);
Datum drop( PG_FUNCTION_ARGS)
{
	text* variableNameArg;
	char* variableName;
	SessionVariable *variable, **higherLvl, *replacement, *aboveReplacement;
	int diff;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.drop(variable_or_constant_name text)"))));
		PG_RETURN_BOOL(false);
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_BOOL(false);
	}
	variableNameArg = PG_GETARG_TEXT_P(0);
	variableName = text_to_cstring(variableNameArg);

	elog(DEBUG1, "@>drop('%s')", variableName);
	if (!strlen(variableName))
	{
		ereport(ERROR,
				(errcode(ERRCODE_ZERO_LENGTH_CHARACTER_STRING), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	higherLvl = &variables;
	while (*higherLvl != NULL)
	{
		variable = *higherLvl;

		diff = strcmp(variableName, variable->name);

		if (diff < 0)
		{
			higherLvl = &variable->prior;
			logVariable(DEBUG4, "searching: prior = ", variable);
		}
		else if (diff > 0)
		{
			higherLvl = &variable->next;
			logVariable(DEBUG4, "searching: next = ", variable);
		}
		else
		{
			break;
		}
	}

	if (*higherLvl == NULL)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("variable or constant \"%s\" does not exists", variableName ))));
		PG_RETURN_BOOL(false);
	}

	logVariable(DEBUG4, "variable to drop = ", variable);
	if (variable->prior == NULL)
	{
		*higherLvl = variable->next;
	}
	else if (variable->next == NULL)
	{
		*higherLvl = variable->prior;
	}
	else
	{
		aboveReplacement = NULL;
		replacement = variable->prior;
		while (replacement->next != NULL)
		{
			aboveReplacement = replacement;
			replacement = replacement->next;
		}
		logVariable(DEBUG4, "replacement before = ", replacement);
		logVariable(DEBUG4, "aboveReplacement before = ", replacement);
		if (aboveReplacement != NULL)
		{
			aboveReplacement->next = replacement->prior;
			logVariable(DEBUG4, "aboveReplacement after = ", aboveReplacement);
		}
		if (variable->prior != replacement)
		{
			replacement->prior = variable->prior;
		}
		replacement->next = variable->next;
		*higherLvl = replacement;
		logVariable(DEBUG4, "replacement after = ", replacement);
	}

	variable->prior = NULL;
	variable->next = NULL;
	removeVariableRecursively(variable);

	deleteVariable(variableNameArg);

	elog(DEBUG1, "@<drop('%s') = true", variableName);
	PG_RETURN_BOOL(true);
}

/*
 * alter_value(variable_constant_name text, value anyelement) returns anyelement
 */
PG_FUNCTION_INFO_V1(alter_value);
Datum alter_value( PG_FUNCTION_ARGS)
{
	char* variableName = NULL;
	SessionVariable* variable;
	bool found;
	bool priorContentIsNull = false;
	Datum priorContent = (Datum) NULL;
	Datum newContent = (Datum) NULL;
	Oid newValueTypeOid;
	int newValueTypeLength;
	bool castFailed;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 2)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.alter_value(variable_name text, value anyelement)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring((text *) PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>alter_value('%s')", variableName);

	if (!strlen(variableName))
	{
		ereport(ERROR,
				(errcode(ERRCODE_ZERO_LENGTH_CHARACTER_STRING), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("Variable \"%s\" does not exists", variableName))));
		PG_RETURN_NULL()
		;
	}

	newValueTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	if (variable->type == newValueTypeOid)
	{
		newValueTypeLength = variable->typeLength;
	}
	else
	{
		newValueTypeLength = getTypeLength(newValueTypeOid);
	}

	/*
	 * To be removed well after the introduction of version 2 to allow people
	 * time to migrate
	 */
	if (version1)
	{
		priorContentIsNull = variable->isNull;
		if (!priorContentIsNull)
		{
			priorContent = coerceOutput(variable->type, variable->typeLength,
					variable->content, newValueTypeOid, &castFailed);
			if (castFailed)
			{
				/*
				 * something went wrong, but that is already logged
				 */
				PG_RETURN_NULL()
				;
			}
		}
	}

	if (PG_ARGISNULL(1))
	{
		newContent = (Datum) NULL;
	}
	else
	{

		if (newValueTypeLength < 0)
		{
			newContent = (Datum) PG_GETARG_VARLENA_P(1);
		}
		else
		{
			newContent = PG_GETARG_DATUM(1);
		}

		/*
		 * Make sure the new content is malloced instead of palloced, and cast to the right type of course.
		 */
		newContent = coerceInput(newValueTypeOid, variable->type,
				variable->typeLength, newContent, &castFailed);
		if (castFailed)
		{
			/*
			 * Something went wrong, but that has already been logged
			 */

			PG_RETURN_NULL()
			;
		}
	}

	if (!variable->isNull
			&& (variable->typeLength < 0 || variable->typeLength > SIZEOF_DATUM))
	{
		/*
		 * The prior content has been malloced instead of palloced, so must be freed here.
		 */
		free((void*) variable->content);
	}
	variable->isNull = PG_ARGISNULL(1);
	variable->content = newContent;

	updateVariable(variable);

	elog(DEBUG1, "@<alter_value('%s')", variableName);

	/*
	 * To be removed well after the introduction of version 2 to allow people
	 * time to migrate
	 */
	if (version1)
	{
		if (priorContentIsNull)
		{
			PG_RETURN_NULL()
			;
		}
		PG_RETURN_DATUM(priorContent);
	}

	PG_RETURN_BOOL(true);
}

/*
 * set(variable_name text, value anyelement) returns boolean
 */
PG_FUNCTION_INFO_V1(set);
Datum set( PG_FUNCTION_ARGS)
{
	char* variableName = NULL;
	SessionVariable* variable;
	bool found;
	bool priorContentIsNull = false;
	Datum priorContent = (Datum) NULL;
	Datum newContent = (Datum) NULL;
	Oid newValueTypeOid;
	int newValueTypeLength;
	bool castFailed;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 2)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.set(variable_name text, value anyelement)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring((text*) PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>set('%s')", variableName);

	if (!strlen(variableName))
	{
		ereport(ERROR,
				(errcode(ERRCODE_ZERO_LENGTH_CHARACTER_STRING), (errmsg("variable name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("variable \"%s\" does not exists", variableName))));
		PG_RETURN_NULL()
		;
	}

	if (variable->isConstant && !isExecutingInitialisationFunction) // the initialisation function is allowed to alter the value of a constant
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),(errmsg("constant \"%s\" cannot be set", variableName))));
		PG_RETURN_NULL()
		;
	}

	newValueTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	if (variable->type == newValueTypeOid)
	{
		newValueTypeLength = variable->typeLength;
	}
	else
	{
		newValueTypeLength = getTypeLength(newValueTypeOid);
	}

	/*
	 * To be removed well after the introduction of version 2 to allow people
	 * time to migrate
	 */
	if (version1)
	{
		priorContentIsNull = variable->isNull;
		if (!priorContentIsNull)
		{
			priorContent = coerceOutput(variable->type, variable->typeLength,
					variable->content, newValueTypeOid, &castFailed);
			if (castFailed)
			{
				/*
				 *  something went wrong, but that is already logged
				 */
				PG_RETURN_NULL()
				;
			}
		}
	}

	if (PG_ARGISNULL(1))
	{
		newContent = (Datum) NULL;
	}
	else
	{

		if (newValueTypeLength < 0)
		{
			newContent = (Datum) PG_GETARG_VARLENA_P(1);
		}
		else
		{
			newContent = PG_GETARG_DATUM(1);
		}

		/*
		 * Make sure the new content is malloced instead of palloced, and cast to the right type of course.
		 */
		newContent = coerceInput(newValueTypeOid, variable->type,
				variable->typeLength, newContent, &castFailed);
		if (castFailed)
		{
			/*
			 * Something went wrong, but that has already been logged
			 */

			PG_RETURN_NULL()
			;
		}
	}

	if (!variable->isNull
			&& (variable->typeLength < 0 || variable->typeLength > SIZEOF_DATUM))
	{
		/*
		 * The prior content has been malloced instead of palloced, so must be freed here.
		 */
		free((void*) variable->content);
	}

	variable->isNull = PG_ARGISNULL(1);
	variable->content = newContent;

	elog(DEBUG1, "@<set('%s')", variableName);

	/*
	 * To be removed well after the introduction of version 2 to allow people
	 * time to migrate
	 */
	if (version1)
	{
		if (priorContentIsNull)
		{
			PG_RETURN_NULL()
			;
		}
		PG_RETURN_DATUM(priorContent);
	}

	PG_RETURN_BOOL(true);
}

/*
 * get(variable_constant_name text) returns anyelement
 */
PG_FUNCTION_INFO_V1(get);
Datum get( PG_FUNCTION_ARGS)
{
	char* variableName = NULL;
	Datum result = (Datum) NULL;
	SessionVariable* variable;
	bool found;
	Oid resultTypeOid;
	bool castFailed;
	CoercionPathType coercionPathType;
	Oid coercionFunctionOid;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 2)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION), (errmsg( "Usage: session_variable.get(variable_or_constant_name text, just_for_type anyelement)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring(PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>get('%s')", variableName);

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("variable or constant '%s' does not exists", variableName ))));
		PG_RETURN_NULL()
		;
	}

	resultTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	if (variable->isNull)
	{
		if (resultTypeOid == variable->type)
		{
			coercionPathType = COERCION_PATH_RELABELTYPE;
		}
		else
		{
			coercionPathType = find_coercion_pathway(variable->type,
					resultTypeOid, COERCION_EXPLICIT, &coercionFunctionOid);
		}
		switch (coercionPathType)
		{
		case COERCION_PATH_RELABELTYPE:
		case COERCION_PATH_FUNC:
		case COERCION_PATH_COERCEVIAIO:
			elog(DEBUG1, "@<get('%s') = NULL", variableName);
			PG_RETURN_NULL()
			;
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("The variable's internal type %s, cannot be cast to type %s", getTypeName(variable->type) ,getTypeName(resultTypeOid) ))));
			PG_RETURN_NULL()
			;
		}
	}

	result = coerceOutput(variable->type, variable->typeLength,
			variable->content, resultTypeOid, &castFailed);

	elog(DEBUG1, "@<get('%s')", variableName);

	PG_RETURN_DATUM(result);
}

/*
 * type_of(variable_or_constant_name text) returns regtype
 */
PG_FUNCTION_INFO_V1(type_of);
Datum type_of( PG_FUNCTION_ARGS)
{
	char* variableName = NULL;
	SessionVariable* variable;
	bool found;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION), (errmsg( "Usage: session_variable.type_of(variable_or_constant_name text)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring(PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>type_of('%s')", variableName);

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("variable or constant '%s' does not exists", variableName ))));
		PG_RETURN_NULL()
		;
	}

	elog(DEBUG1, "@<type_of('%s')", variableName);

	PG_RETURN_OID(variable->type);
}

/*
 * exists(variable_constant_name text) returns bool
 */
PG_FUNCTION_INFO_V1(exists);
Datum exists( PG_FUNCTION_ARGS)
{
	bool found;
	char* variableName;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION), (errmsg( "Usage: session_variable.exists(variable_or_constant_name text)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring((text*) PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>exists('%s')", variableName);

	searchVariable(variableName, &variables, &found);

	elog(DEBUG1, "@<exists('%s') = %d", variableName, found);

	PG_RETURN_BOOL(found);
}

/*
 * is_constant(variable_constant_name text) returns bool
 */
PG_FUNCTION_INFO_V1(is_constant);
Datum is_constant( PG_FUNCTION_ARGS)
{
	char* variableName;
	SessionVariable* variable;
	bool found;

	if (virgin)
	{
		reload();
	}

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.exists(variable_or_constant_name text)"))));
		PG_RETURN_NULL()
		;
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable or constant name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring((text*) PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>is_constant('%s')", variableName);

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("variable or constant '%s' does not exists", variableName ))));
		PG_RETURN_NULL()
		;
	}

	elog(DEBUG1, "@<is_constant('%s') = %d", variableName,
	variable->isConstant);

	PG_RETURN_BOOL(variable->isConstant);
}

/*
 * init() returns integer
 */
PG_FUNCTION_INFO_V1(init);
Datum init( PG_FUNCTION_ARGS)
{
	int result;

	if (PG_NARGS() != 0)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.init()"))));
		PG_RETURN_INT32(-1);
	}

	elog(DEBUG1, "@>init()");

	result = reload();

	elog(DEBUG1, "@<init() = %d", result);

	PG_RETURN_INT32(result);
}

/*
 * function session_variable.get_session_variable_version() returns text.
 *
 * This function returns the current version of this database extension
 */
PG_FUNCTION_INFO_V1(get_session_variable_version);
Datum get_session_variable_version( PG_FUNCTION_ARGS)
{
	Datum pg_versioning_version;

	if (virgin)
	{
		reload();
	}

	pg_versioning_version = (Datum) palloc(
	VARHDRSZ + strlen(sessionVariableVersion));
	SET_VARSIZE(pg_versioning_version,
			VARHDRSZ + strlen(sessionVariableVersion));
	memcpy(VARDATA(pg_versioning_version), sessionVariableVersion,
			strlen(sessionVariableVersion));
	PG_RETURN_DATUM(pg_versioning_version);
}

/*
 * Updates the variable and all it's children in the session_variable.variables
 * table.
 * This function is considered part of an update script. make sure
 * session_variable.init() is invoked before using this. Otherwise occasional
 * local values will be stored and used as default value.
 *
 * @param SessionVariable* var - the variable to update recursively
 */
void updateRecursively(SessionVariable* var)
{
	if (!var)
	{
		return;
	}
	updateRecursively(var->prior);
	updateRecursively(var->next);
	updateVariable(var);
}

Datum upgrade_1_to_2(PG_FUNCTION_ARGS);
/*
 * Updates all loaded variables - storing them in the right format for version 2
 */
PG_FUNCTION_INFO_V1(upgrade_1_to_2);
Datum upgrade_1_to_2(PG_FUNCTION_ARGS)
{
	elog(LOG, "Upgrade session variables from version 1 to version 2");

	deserialize = &deserializeV2;
	serialize = &serializeV2;
	initialValueTypeOid = TEXTOID;
	version1 = false;

	updateRecursively(variables);

	PG_RETURN_VOID() ;
}
