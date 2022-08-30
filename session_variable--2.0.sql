/*
 * Copyright (c) Splendid Data Product Development B.V. 2013 - 2022
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

do $$
begin
    if not exists (
        select role_name
        from information_schema.enabled_roles
        where role_name = 'session_variable_user_role')
    then
        create role session_variable_user_role;
    end if;
    if not exists (
        select role_name
        from information_schema.enabled_roles
        where role_name = 'session_variable_administrator_role')
    then
        create role session_variable_administrator_role
            in role session_variable_user_role;
    end if;
end; $$;

comment on schema session_variable is 'Belongs to the session_variable extension';
grant usage on schema session_variable to session_variable_user_role;

create table variables 
(  variable_name                text          not null 
                                              primary key
                                              collate "C"
,  created_timestamp            timestamp     not null
,  created_by                   text          not null
,  last_updated_timestamp       timestamp     not null
,  last_updated_by              text          not null
,  is_constant                  boolean       not null
,  variable_type_namespace      name          not null
,  variable_type_name           name          not null
,  initial_value                text
);
select pg_catalog.pg_extension_config_dump('variables', '');
comment on table variables is 'holds constant values and the initial values of session variables';

create function variables_bi()
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
drop trigger if exists variables_bi on variables;
create trigger variables_bi
before insert on variables 
for each row execute procedure variables_bi();

create function variables_bu()
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
drop trigger if exists variables_bu on variables;
create trigger variables_bu
before update on variables 
for each row execute procedure variables_bu();

create function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ) returns boolean
    as 'session_variable.so', 'create_variable' language C security definer;
comment on function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ) is 'create a session variable with null as initial value';
grant execute on function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    )
    to session_variable_administrator_role;     

create function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ,   initial_value               anyelement
    ) returns boolean
    as 'session_variable.so', 'create_variable' language C security definer;
comment on function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ,   initial_value               anyelement
    ) is 'create a session variable with initial value';
grant execute on function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ,   initial_value               anyelement
    )
    to session_variable_administrator_role;

create function create_constant
    (   constant_name               text
    ,   constant_type               regtype
    ,   constant_value              anyelement
    ) returns boolean
    as 'session_variable.so', 'create_constant' language C security definer;
comment on function create_constant
    (   constant_name               text
    ,   constant_type               regtype
    ,   constant_value              anyelement
    ) is 'create a constant';
grant execute on function create_constant
    (   constant_name               text
    ,   constant_type               regtype
    ,   constant_value              anyelement
    )
    to session_variable_administrator_role;

create function alter_value
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

create function drop(variable_or_constant_name text)
    returns boolean
    as 'session_variable.so', 'drop' language C security definer;
comment on function drop(variable_or_constant_name text)
    is 'drop the constant or the session variable with the specified name';
grant execute on function drop(variable_or_constant_name text)
    to session_variable_administrator_role;

create function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement
    as 'session_variable.so', 'get' language C security definer cost 2;
comment on function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    ) is 'Get the content of a constant or a session variable';
grant execute on function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    to session_variable_user_role;

create function set(variable_name text, new_value anyelement)
    returns boolean 
    as 'session_variable.so', 'set' language C security definer cost 2;
comment on function set(variable_name text, new_value anyelement) is
    'Update the value of a session variable. The changed value will be visible in the curent session only';
grant execute on function set(variable_name text, new_value anyelement)
    to session_variable_user_role;

create function "exists"(variable_name text) 
    returns boolean
    as 'session_variable.so', 'exists' language C security definer cost 2;
comment on function "exists"(variable_name text) is
    'Checks if a constant or session variable with the specified name exists';
grant execute on function "exists"(variable_name text)
    to session_variable_user_role;
    
create function type_of(variable_or_constant_name text)
    returns regtype
    as 'session_variable.so', 'type_of' language C security definer cost 2;
comment on function type_of(variable_or_constant_name text) is
    'Returns the datatype of the value of the specified constant or session variable'; 
grant execute on function type_of(variable_or_constant_name text)
    to session_variable_user_role;
    
create function is_constant(variable_or_constant_name text)
    returns boolean
    as 'session_variable.so', 'is_constant' language C security definer cost 2;
comment on function is_constant(variable_or_constant_name text) is
    'Returns true if the specified constant or variable appears to be a constant'
    ' or false if it happens to be a session variable'; 
grant execute on function is_constant(variable_or_constant_name text)
    to session_variable_user_role;

create function init()
    returns integer
    as 'session_variable.so', 'init' language C security definer;
comment on function init() is 
    'Reloads all constants and session variables from the variables table, thus reverting all local changes';

create function get_session_variable_version()
    returns varchar
    as 'session_variable.so', 'get_session_variable_version' language C security definer cost 1;
comment on function get_session_variable_version() is 
    'Reurns the version of the session_variable database extension';
grant execute on function get_session_variable_version() 
    to session_variable_user_role;

create function is_executing_variable_initialisation()
    returns boolean
    as 'session_variable.so', 'is_executing_variable_initialisation' language C security definer cost 1;
comment on function is_executing_variable_initialisation() is 
    'Reurns true if a function called session_variable.variable_initialisation() currently being invoked on behalf of session_variable initialisation code';
    
create function session_variable.dump(do_truncate boolean default true)
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
comment on function dump(do_truncate boolean) is 
    'Reurns a create script for all constants and variables';
grant execute on function dump(do_truncate boolean) 
    to session_variable_administrator_role;
    
revoke all on all functions in schema session_variable from public;
grant execute on function init() to public;
grant execute on function is_executing_variable_initialisation() 
    to public;