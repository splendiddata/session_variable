# session_variable
The session_variable Postgres database extension provides a way to create and maintain session scoped variables and constants, more or less like Oracle's global variables.

Install as a normal Posrgres database extension:
 - Make sure pg_config points to the right places
 - execute make
 - execute sudo make install installcheck
and then in the Porstres database execute:
 - create extension session_variable;

For more information please see <a href="https://github.com/splendiddata/session_variable/blob/master/session_variable.html">session_variable.html</a>.
