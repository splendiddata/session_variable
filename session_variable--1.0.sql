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
 
create schema if not exists session_variable;
comment on schema session_variable is 'Belongs to the session_variable extension';
grant usage on schema session_variable to session_variable_user_role;

create table if not exists variables 
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
,  initial_value                bytea
);
select pg_catalog.pg_extension_config_dump('variables', '');
comment on table variables is 'holds constant values and the initial values of session variables';

create or replace function variables_bi()
returns trigger as
$body$
begin
    new.created_timestamp = current_timestamp;
    new.last_updated_timestamp = current_timestamp;
    new.created_by = current_user;
    new.last_updated_by = current_user;
    return new;
end;
$body$
language plpgsql
security definer;
drop trigger if exists variables_bi on variables;
create trigger variables_bi
before insert on variables 
for each row execute procedure variables_bi();

create or replace function variables_bu()
returns trigger as
$body$
begin
    new.last_updated_timestamp = current_timestamp;
    new.last_updated_by = current_user;
    return new;
end;
$body$
language plpgsql
security definer;
drop trigger if exists variables_bu on variables;
create trigger variables_bu
before update on variables 
for each row execute procedure variables_bu();

create or replace function create_variable
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

create or replace function create_variable
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

create or replace function create_constant
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

create or replace function alter_value
    (   variable_or_constant_name   text
    ,   variable_or_constant_value  anyelement
    ) returns anyelement
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

create or replace function drop(variable_or_constant_name text)
    returns boolean
    as 'session_variable.so', 'drop' language C security definer;
comment on function drop(variable_or_constant_name text)
    is 'drop the constant or the session variable with the specified name';
grant execute on function drop(variable_or_constant_name text)
    to session_variable_administrator_role;

create or replace function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement
    as 'session_variable.so', 'get' language C security definer;
comment on function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    ) is 'Get the content of a constant or a session variable';
grant execute on function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    to session_variable_user_role;

create or replace function set(varialbe_name text, new_value anyelement)
    returns anyelement 
    as 'session_variable.so', 'set' language C security definer;
comment on function set(varialbe_name text, new_value anyelement) is
    'Update the value of a session variable. The changed value will be visible in the curent session only';
grant execute on function set(varialbe_name text, new_value anyelement)
    to session_variable_user_role;

create or replace function "exists"(variable_name text) 
    returns boolean
    as 'session_variable.so', 'exists' language C security definer;
comment on function "exists"(variable_name text) is
    'Checks if a constant or session variable with the specified name exists';
grant execute on function "exists"(variable_name text)
    to session_variable_user_role;
    
create or replace function type_of(variable_or_constant_name text)
    returns regtype
    as 'session_variable.so', 'type_of' language C security definer;
comment on function type_of(variable_or_constant_name text) is
    'Returns the datatype of the value of the specified constant or session variable'; 
grant execute on function type_of(variable_or_constant_name text)
    to session_variable_user_role;
    
create or replace function is_constant(variable_or_constant_name text)
    returns boolean
    as 'session_variable.so', 'is_constant' language C security definer;
comment on function is_constant(variable_or_constant_name text) is
    'Returns true if the specified constant or variable appears to be a constant'
    ' or false if it happens to be a session variable'; 
grant execute on function is_constant(variable_or_constant_name text)
    to session_variable_user_role;

create or replace function init()
    returns integer
    as 'session_variable.so', 'init' language C security definer;
comment on function init() is 
    'Reloads all constants and session variables from the variables table, thus reverting all local changes';
grant execute on function init() 
    to session_variable_user_role;
    
revoke all on all functions in schema session_variable from public;