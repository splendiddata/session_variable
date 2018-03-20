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

#ifndef SESSION_VARIABLE_H
#define SESSION_VARIABLE_H


static char* sessionVariableVersion = "1.0.2";

/*
 * Loads the session variables from the session_variable.variables table
 */
void _PG_init(void);

/*
 * Functions defined in this file
 */
extern Datum alter_value(PG_FUNCTION_ARGS);
extern Datum create_constant(PG_FUNCTION_ARGS);
extern Datum create_variable(PG_FUNCTION_ARGS);
extern Datum drop(PG_FUNCTION_ARGS);
extern Datum exists(PG_FUNCTION_ARGS);
extern Datum get(PG_FUNCTION_ARGS);
extern Datum init(PG_FUNCTION_ARGS);
extern Datum is_constant(PG_FUNCTION_ARGS);
extern Datum set(PG_FUNCTION_ARGS);
extern Datum type_of(PG_FUNCTION_ARGS);
extern Datum get_session_variable_version( PG_FUNCTION_ARGS);

typedef struct SessionVariable
{
	struct SessionVariable* prior;
	struct SessionVariable* next;
	char *name;
	Oid type;
	Datum content;
	bool isConstant;
	int typeLength;
	bool isNull;
} SessionVariable;

#define getTypeName(typeOid) (DatumGetCString(DirectFunctionCall1(regtypeout, typeOid)))

#endif   /* SESSION_VARIABLE_H */
