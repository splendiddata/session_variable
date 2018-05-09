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

/*
 * Load all variables with their initial value into memory
 */
select session_variable.init();

/*
 * Alter the table definition
 */
alter table session_variable.variables
    alter column initial_value type text;
    
/*
 * Update the initial value of all variables from memory in the new format
 */
create function upgrade_1_to_2() returns void
    as 'session_variable.so', 'upgrade_1_to_2' language C security definer;
select upgrade_1_to_2();
drop function upgrade_1_to_2();

/*
 * Alter the definition of function alter_value() to return boolean instead of
 * anyelement
 */
drop function alter_value
    (   variable_or_constant_name   text
    ,   variable_or_constant_value  anyelement
    );
create or replace function alter_value
    (   variable_or_constant_name   text
    ,   variable_or_constant_value  anyelement
    ) returns boolean
    as 'session_variable.so', 'alter_value' language C security definer;
comment on function alter_value
    (   variable_or_constant_name   text
    ,   variable_or_constant_value  anyelement
    ) is 'alter the value of a constant or the initial value of a session variable';
grant execute on function alter_value
    (   variable_or_constant_name   text
    ,   variable_or_constant_value  anyelement
    )
    to session_variable_administrator_role;

/*
 * Alter the definition of function set() to return boolean instead of 
 * anyelement
 */
drop function set(variable_name text, new_value anyelement);
create or replace function set(variable_name text, new_value anyelement)
    returns boolean 
    as 'session_variable.so', 'set' language C security definer cost 2;
comment on function set(variable_name text, new_value anyelement) is
    'Update the value of a session variable. The changed value will be visible in the curent session only';
grant execute on function set(variable_name text, new_value anyelement)
    to session_variable_user_role;
    
/*
 * re-define the dump procedure
 */ 
create or replace function session_variable.dump(do_truncate boolean default true)
  returns setof text AS
$$
declare
    var_cursor cursor is 
        select variable_name
			 , is_constant
			 , var.variable_type_namespace
			   || '.'
			   || case
			      when etyp.typname is not null
			       and typ.typname ~ ('^_+' || etyp.typname || '$')
			       then etyp.typname || '[]'
			      else typ.typname 
			      end type_name
			 , initial_value  
        from session_variable.variables var
		join pg_catalog.pg_namespace nsp 
		    on var.variable_type_namespace = nsp.nspname 
        join pg_catalog.pg_type typ 
            on typ.typnamespace = nsp.oid
            and var.variable_type_name = typ.typname
	    left join pg_catalog.pg_type etyp
	        on typ.typelem = etyp.oid 
        order by variable_name;
    var_rec record;
    sql     text;
    var_content text;
begin
    if do_truncate
    then
        return next 'truncate table session_variable.variables;';
    end if;
    return next 'select session_variable.init();';
    for var_rec in var_cursor loop
        return next format ( 'select session_variable.create_'
                              || case var_rec.is_constant 
                                 when true then 'constant' 
                                 else 'variable'
                                 end
                              || '(%L, %L::regtype, %L::%s)'
                              || case 
                                 when do_truncate then ';'
                                 else ' where not session_variable.exists(%L);'
                                 end
                           , var_rec.variable_name
                           , var_rec.type_name
                           , var_rec.initial_value
                           , var_rec.type_name
                           , var_rec.variable_name
                           );
    end loop;
end;
$$ language plpgsql;

/*
 * Make created_by and last_updated_by reflect the session_user, not the
 * current_user 
 */
create or replace function variables_bi()
returns trigger as
$body$
begin
    new.created_timestamp = current_timestamp;
    new.last_updated_timestamp = current_timestamp;
    new.created_by = session_user;
    new.last_updated_by = session_user;
    return new;
end;
$body$
language plpgsql
security definer;

create or replace function variables_bu()
returns trigger as
$body$
begin
    new.last_updated_timestamp = current_timestamp;
    new.last_updated_by = session_user;
    return new;
end;
$body$
language plpgsql
security definer;
