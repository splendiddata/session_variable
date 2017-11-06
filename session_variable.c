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
#include "storage/fd.h"
#include <sys/stat.h>
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/lsyscache.h"

#include "session_variable.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC
;
#endif

static Oid pg_catalogOID;
static SessionVariable* variables = NULL;

/*
 * Finds the type name in the type cache and returns whether or not it is a varlena type
 *
 * @param Oid typeOid - Identification of the type
 * @return bool true if the type is a variable length type
 */
bool isTypeVarlena(Oid typeOid);
bool isTypeVarlena(Oid typeOid)
{

	HeapTuple typTup;
	Form_pg_type typ;
	bool result;

	typTup = SearchSysCache1(TYPEOID, ObjectIdGetDatum(typeOid));
	if (!HeapTupleIsValid(typTup))
	{
		elog(ERROR, "cache lookup failed for type %u", typeOid);
		return false;
	}
	typ = (Form_pg_type) GETSTRUCT(typTup);

	result = typ->typlen == -1;

	ReleaseSysCache(typTup);

	return result;
}

void logVariable(int logLevel, char* leadingText, SessionVariable* variable);
void logVariable(int logLevel, char* leadingText, SessionVariable* variable)
{
	if (variable == NULL)
	{
		elog(logLevel, "%s 0:null", leadingText);
	}
	elog(logLevel,
			"%s %p:%s, type=%d,%s, isVarlena=%d, isConstant=%d, isNull=%d, prior=%p:%s, next=%p:%s",
			leadingText, variable, variable->name, variable->type,
			getTypeName(variable->type), variable->isVarlena,
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
void removeVariableRecursively(SessionVariable* v);
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
	if (v->isVarlena)
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
 * @param bool isVarlena: Is this a variable length data type
 * @param Datum value or null if none.
 */
SessionVariable* createVariable(text* variableName, bool isConst, Oid valueType,
		bool isVarlena, bool isNull, Datum value);
SessionVariable* createVariable(text* variableName, bool isConst, Oid valueType,
		bool isVarlena, bool isNull, Datum value)
{
	/*
	 * The variable is to be stored as session variable (read: static variable).
	 * So we do not use palloc() here, but malloc() instead.
	 */
	SessionVariable* result = (SessionVariable*) malloc(
			sizeof(SessionVariable));

	elog(DEBUG3,
			"createVariable(%s, isConst=%d, valueType=%d, isVarlena=%d, isNull=%d, value)",
			text_to_cstring(variableName), isConst, valueType, isVarlena,
			isNull);

	result->prior = NULL;
	result->next = NULL;

	result->name = (char*) malloc(VARSIZE(variableName) - VARHDRSZ + 1);
	memcpy(result->name, VARDATA(variableName),
	VARSIZE(variableName) - VARHDRSZ);
	result->name[VARSIZE(variableName) - VARHDRSZ] = '\0';
	if (isNull)
	{
		result->content = (Datum) NULL;
	}
	else if (isVarlena)
	{
		result->content = (Datum) malloc(VARSIZE(value));
		memcpy((void*) result->content, (void*) value, VARSIZE(value));
	}
	else
	{
		result->content = value;
	}
	result->type = valueType;
	result->isVarlena = isVarlena;
	result->isConstant = isConst;
	result->isNull = isNull;

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
void buildBTree(void);
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

/*
 * Walks through the session_variable.variables table to build the SessionVariable b-tree in variables.
 *
 * @return int The number or SessionVariables created
 */
int reload(void);
int reload()
{
	char* sql =
			"select variable_name"
					", is_constant"
					", typ.oid"
					", initial_value"
					" from session_variable.variables var"
					" join pg_catalog.pg_namespace nsp on var.variable_type_namespace = nsp.nspname"
					" join pg_catalog.pg_type typ on nsp.oid = typ.typnamespace and var.variable_type_name = typ.typname"
					" order by variable_name";
	text* variableName = NULL;
	bool isConstValue = false;
	Oid valueType = NULL;
	Datum valueByteArray = NULL;
	Datum value = NULL;
	bool isNull;
	Portal cursor = NULL;
	int nrVariables = 0;
	SessionVariable** nextVar = &variables;
	bool isVarlena = false;

	/*
	 * Clear the old content (if any).
	 */
	removeVariableRecursively(variables);
	variables = NULL;

	SPI_connect();

	/*
	 * Walk through the session_variable.variables table
	 */
	cursor = SPI_cursor_open_with_args(NULL, sql, 0, NULL, NULL, NULL, true, 1);
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
		isVarlena = isTypeVarlena(valueType);
		value = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 4,
				&isNull);
		if (!isNull)
		{
			valueByteArray = (Datum) PG_DETOAST_DATUM(value);
			value = (Datum) VARDATA(valueByteArray);
			if ((isVarlena
					&& VARSIZE(valueByteArray) - VARHDRSZ != VARSIZE(value))
					|| (!isVarlena
							&& VARSIZE(valueByteArray) - VARHDRSZ
									!= SIZEOF_DATUM))
			{
				/*
				 * The Datum containing the content of the value is wrapped in a bytea datum and stored.
				 * So the size of the content must match the content size of the wrapping bytea (= VARSIZE - VARHDRSZ).
				 * If this is not the case, someone has manually altered the content.
				 */
				elog(ERROR, "Someone has been messing around with variable %s",
						DatumGetPointer(variableName));

				removeVariableRecursively(variables);
				SPI_cursor_close(cursor);
				SPI_finish();
				return 0;
			}
			if (!isVarlena)
			{
				Datum tmp;
				memcpy(&tmp, (void*) value, SIZEOF_DATUM);
				value = tmp;
			}
		}

		/*
		 * Create the new value and link it to its predecessor.
		 */
		*nextVar = createVariable(variableName, isConstValue, valueType,
				isVarlena, isNull, value);
		nextVar = &(*nextVar)->next;

		nrVariables++;
		SPI_cursor_fetch(cursor, true, 1);
	}
	SPI_cursor_close(cursor);

	SPI_finish();

	buildBTree();

	elog(DEBUG1, "reload() = %d", nrVariables);
	return nrVariables;
}

/*
 * Inserts the variable into the session_variable.variables table
 *
 * @param SessionVariable* variable
 * @return bool: true if ok
 */
bool insertVariable(SessionVariable* variable);
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
	oid[3] = BYTEAOID;
	if (variable->isNull)
	{
		val[3] = PointerGetDatum(NULL);
		nulls[3] = 'n';
	}
	else
	{
		Datum contentWrapper;
		if (variable->isVarlena)
		{
			contentWrapper = (Datum) palloc(
			VARHDRSZ + VARSIZE(variable->content));
			SET_VARSIZE(contentWrapper, VARHDRSZ + VARSIZE(variable->content));
			memcpy(VARDATA(contentWrapper), (void*) variable->content,
					VARSIZE(variable->content));
		}
		else
		{
			contentWrapper = (Datum) palloc(VARHDRSZ + SIZEOF_DATUM);
			SET_VARSIZE(contentWrapper, VARHDRSZ + SIZEOF_DATUM);
			memcpy(VARDATA(contentWrapper), &variable->content,
			SIZEOF_DATUM);
		}
		val[3] = (Datum) contentWrapper;
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
void updateVariable(SessionVariable* variable);
void updateVariable(SessionVariable* variable)
{
	char* sql = "update session_variable.variables"
			" set initial_value = $1"
			" where variable_name = $2";
	int32 nrArgs = 2;
	Oid oid[nrArgs];
	Datum val[nrArgs];
	char nulls[nrArgs];

	oid[0] = BYTEAOID;
	if (variable->isNull)
	{
		val[0] = PointerGetDatum(NULL);
		nulls[0] = 'n';
	}
	else
	{
		Datum contentWrapper;
		if (variable->isVarlena)
		{
			contentWrapper = (Datum) palloc(
			VARHDRSZ + VARSIZE(variable->content));
			SET_VARSIZE(contentWrapper, VARHDRSZ + VARSIZE(variable->content));
			memcpy(VARDATA(contentWrapper), (void*) variable->content,
					VARSIZE(variable->content));
		}
		else
		{
			contentWrapper = (Datum) palloc(VARHDRSZ + SIZEOF_DATUM);
			SET_VARSIZE(contentWrapper, VARHDRSZ + SIZEOF_DATUM);
			memcpy((void*) VARDATA(contentWrapper), &variable->content,
			SIZEOF_DATUM);
		}
		val[0] = (Datum) contentWrapper;
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
void deleteVariable(text* variablename);
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
		bool* found);
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
 * Stores the variable with the specified data in memory and in the session_varialbe.variables table
 *
 * @param text* variableName: Name of the variable to be stored
 * @param bool isConst: Is this variable to be treated as constant
 * @param Oid valueType: Data type of the variable content
 * @param bool isVarlena: Is this a variable length data type
 * @param bool isNull: Is the content NULL
 * @param Datum value: The value of the variable, may be NULL
 * @return bool: true if ok
 */
bool saveNewVariable(text* variableName, bool isConst, Oid valueType,
		bool isVarlena, bool isNull, Datum value);
bool saveNewVariable(text* variableName, bool isConst, Oid valueType,
		bool isVarlena, bool isNull, Datum value)
{
	bool found;
	SessionVariable* parentLevel;
	SessionVariable* variable;
	char* varName = text_to_cstring((text*) variableName);

	parentLevel = searchVariable(varName, &variables, &found);
	if (found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNIQUE_VIOLATION) , (errmsg("Variable \"%s\" already exists", varName ))));
		return false;
	}
	variable = createVariable(variableName, isConst, valueType, isVarlena,
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
 * Finds the oid of the "pg_catalog" namespace
 */
void _PG_init(void)
{
	pg_catalogOID = get_namespace_oid("pg_catalog", false);
	reload();
}

/*
 * create_variable(variable_name text, variable_type regtype) returns boolean
 * create_variable(variable_name text, variable_type regtype, initial_value anyelement) returns boolean
 */
PG_FUNCTION_INFO_V1(create_variable);
Datum create_variable( PG_FUNCTION_ARGS)
{
	text* variableName = NULL;
	Oid type = NULL;
	Oid typeOid = NULL;
	Datum content = NULL;
	Oid contentTypeOid = NULL;
	bool isVarlena = false;
	bool isNull = false;
	bool result;

	if (PG_NARGS() < 2 || PG_NARGS() > 3)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.create_variable(variable_name text, variable_type regtype) " "or session_variable.create_variable(variable_name text, variable_type regtype, initial_value anyelement)"))));
		PG_RETURN_BOOL(false);
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_name must be filled"))));
		PG_RETURN_BOOL(false);
	}
	else
	{
		variableName = PG_GETARG_TEXT_P(0);
	}
	if (PG_ARGISNULL(1))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_type must be filled"))));
		PG_RETURN_BOOL(false);
	}
	else
	{
		type = PG_GETARG_OID(1);
		typeOid = DatumGetObjectId(type);
		isVarlena = isTypeVarlena(typeOid);
	}

	elog(DEBUG1, "@>create_variable('%s')", text_to_cstring(variableName));

	isNull = PG_NARGS() < 3 || PG_ARGISNULL(2);
	if (!isNull)
	{
		contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 2);
		if (typeOid != contentTypeOid)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("initial_value should be of type %s, but is of type %s", getTypeName(typeOid), getTypeName(contentTypeOid) ))));
			PG_RETURN_BOOL(false);
		}
		if (isVarlena)
		{
			content = (Datum) PG_GETARG_VARLENA_P(2);
		}
		else
		{
			content = PG_GETARG_DATUM(2);
		}
	}

	result = saveNewVariable(variableName, false, type, isVarlena, isNull,
			content);

	elog(DEBUG1, "@<create_variable('%s')", text_to_cstring(variableName));

	PG_RETURN_BOOL(result);
}

/*
 * create_constant(constant_name text, constant_type regtype, value anyelement) returns boolean
 */
PG_FUNCTION_INFO_V1(create_constant);
Datum create_constant( PG_FUNCTION_ARGS)
{
	text* variableName = NULL;
	Oid type = NULL;
	Oid typeOid = NULL;
	Datum content = NULL;
	Oid contentTypeOid = NULL;
	bool isVarlena = false;
	bool result;

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
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("constant_name must be filled"))));
		PG_RETURN_BOOL(false);;
	}
	else
	{
		variableName = PG_GETARG_TEXT_P(0);
	}

	if (PG_ARGISNULL(1))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("constant_type must be filled"))));
		PG_RETURN_BOOL(false);
	}
	else
	{
		type = PG_GETARG_OID(1);
		typeOid = DatumGetObjectId(type);
		isVarlena = isTypeVarlena(typeOid);
	}

	elog(DEBUG1, "@>create_constant('%s')", text_to_cstring(variableName));

	if (!PG_ARGISNULL(2))
	{
		contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 2);
		if (typeOid != contentTypeOid)
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("value must be of type %s, but is of type %s", getTypeName(typeOid) ,getTypeName(contentTypeOid) ))));
			PG_RETURN_BOOL(false);
		}
		if (isVarlena)
		{
			content = (Datum) PG_GETARG_VARLENA_P(2);
		}
		else
		{
			content = PG_GETARG_DATUM(2);
		}
	}

	result = saveNewVariable(variableName, true, type, isVarlena,
			PG_ARGISNULL(2), content);

	elog(DEBUG1, "@<create_constant('%s')", text_to_cstring(variableName));

	PG_RETURN_BOOL(result);
}

/*
 * drop(variable_constant_name text) returns anyelement
 */
PG_FUNCTION_INFO_V1(drop);
Datum drop( PG_FUNCTION_ARGS)
{
	text* variableNameArg;
	char* variableName;
	SessionVariable *variable, **higherLvl, *replacement, *aboveReplacement;
	int diff;

	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_FUNCTION),(errmsg( "Usage: session_variable.drop(variable_or_constant_name text)"))));
		PG_RETURN_BOOL(false);
	}

	if (PG_ARGISNULL(0))
	{
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_or_constant_name must be filled"))));
		PG_RETURN_BOOL(false);
	}
	variableNameArg = PG_GETARG_TEXT_P(0);
	variableName = text_to_cstring(variableNameArg);

	elog(DEBUG1, "@>drop('%s')", variableName);

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
	Oid contentTypeOid = NULL;
	SessionVariable* variable;
	bool found;
	bool priorContentIsNull = false;
	Datum priorContent = NULL;
	DatumPtr newContent = NULL;

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
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_or_constant_name must be filled"))));
		PG_RETURN_NULL()
		;
	}

	variableName = text_to_cstring((text *) PG_GETARG_TEXT_P(0));

	elog(DEBUG1, "@>alter_value('%s')", variableName);

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("Variable \"%s\" does not exists", variableName))));
		PG_RETURN_NULL()
		;
	}

	contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	if (variable->type != contentTypeOid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("value should be of type %s, but is of type %s", getTypeName(variable->type) ,getTypeName(contentTypeOid) ))));
		PG_RETURN_NULL()
		;
	}

	priorContentIsNull = variable->isNull;
	if (!priorContentIsNull)
	{
		if (variable->isVarlena)
		{
			priorContent = (Datum) palloc(VARSIZE(variable->content));
			memcpy((void*) priorContent, (void*) variable->content,
					VARSIZE(variable->content));
			free((void*) variable->content);
		}
		else
		{
			priorContent = variable->content;
		}
	}

	variable->isNull = PG_ARGISNULL(1);

	if (variable->isNull)
	{
		variable->content = (Datum) NULL;
	}
	else if (variable->isVarlena)
	{
		newContent = (DatumPtr) PG_GETARG_VARLENA_P(1);
		variable->content = (Datum) malloc(VARSIZE(newContent));
		memcpy((void*) variable->content, newContent, VARSIZE(newContent));
	}
	else
	{
		variable->content = PG_GETARG_DATUM(1);
	}

	updateVariable(variable);

	elog(DEBUG1, "@<alter_value('%s')", variableName);

	if (priorContentIsNull)
	{
		PG_RETURN_NULL()
		;
	}
	PG_RETURN_DATUM(priorContent);
}

/*
 * set(variable_name text, value anyelement) returns anyelement
 */
PG_FUNCTION_INFO_V1(set);
Datum set( PG_FUNCTION_ARGS)
{
	char* variableName = NULL;
	Oid contentTypeOid = NULL;
	SessionVariable* variable;
	bool found;
	bool priorContentIsNull = false;
	Datum priorContent = NULL;
	DatumPtr newContent = NULL;

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

	variable = searchVariable(variableName, &variables, &found);
	if (!found)
	{
		ereport(ERROR,
				(errcode(ERRCODE_NO_DATA),(errmsg("variable \"%s\" does not exists", variableName))));
		PG_RETURN_NULL()
		;
	}

	if (variable->isConstant)
	{
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),(errmsg("constant \"%s\" cannot be set", variableName))));
		PG_RETURN_NULL()
		;
	}

	contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	if (variable->type != contentTypeOid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("value must be of type %s, but is of type %s", getTypeName(variable->type) ,getTypeName(contentTypeOid) ))));
		PG_RETURN_NULL()
		;
	}

	priorContentIsNull = variable->isNull;
	if (!priorContentIsNull)
	{
		if (variable->isVarlena)
		{
			priorContent = (Datum) palloc(VARSIZE(variable->content));
			memcpy((void*) priorContent, (void*) variable->content,
					VARSIZE(variable->content));
			free((void*) variable->content);
		}
		else
		{
			priorContent = variable->content;
		}
	}

	variable->isNull = PG_ARGISNULL(1);
	if (variable->isNull)
	{
		variable->content = (Datum) NULL;
	}
	else if (variable->isVarlena)
	{
		newContent = (DatumPtr) PG_GETARG_VARLENA_P(1);
		variable->content = (Datum) malloc(VARSIZE(newContent));
		memcpy((void*) variable->content, newContent, VARSIZE(newContent));
	}
	else
	{
		variable->content = PG_GETARG_DATUM(1);
	}

	elog(DEBUG1, "@<set('%s')", variableName);

	if (priorContentIsNull)
	{
		PG_RETURN_NULL()
		;
	}
	PG_RETURN_DATUM(priorContent);
}

/*
 * get(variable_constant_name text) returns anyelement
 */
PG_FUNCTION_INFO_V1(get);
Datum get( PG_FUNCTION_ARGS)
{
	char* variableName = NULL;
	Datum content = NULL;
	SessionVariable* variable;
	bool found;
	Oid contentTypeOid;

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
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_or_constant_name must be filled"))));
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

	contentTypeOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
	if (variable->type != contentTypeOid)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE), (errmsg("please invoke as session_variable.get('%s', null::%s)", variableName, getTypeName(variable->type) ))));
		PG_RETURN_NULL()
		;
	}

	if (variable->isNull)
	{
		elog(DEBUG1, "@<get('%s') = NULL", variableName);
		PG_RETURN_NULL()
		;
	}

	if (variable->isVarlena)
	{
		content = (Datum) palloc(VARSIZE(variable->content));
		memcpy((void*) content, (void*) variable->content,
				VARSIZE(variable->content));
	}
	else
	{
		content = variable->content;
	}

	elog(DEBUG1, "@<get('%s')", variableName);

	PG_RETURN_DATUM(content);
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
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_or_constant_name must be filled"))));
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
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_or_constant_name must be filled"))));
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
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), (errmsg("variable_or_constant_name must be filled"))));
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
