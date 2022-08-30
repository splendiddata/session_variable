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

create function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ) returns boolean
    as 'session_variable', 'create_variable' language C security definer;

create function create_variable
    (   variable_name               text
    ,   variable_type               regtype
    ,   initial_value               anyelement
    ) returns boolean
    as 'session_variable', 'create_variable' language C security definer;

create function create_constant
    (   constant_name               text
    ,   constant_type               regtype
    ,   constant_value              anyelement
    ) returns boolean
    as 'session_variable', 'create_constant' language C security definer;

create function alter_value
    (   variable_or_constant_name   text
    ,   variable_or_constant_value  anyelement
    ) returns boolean
    as 'session_variable', 'alter_value' language C security definer;

create function drop(variable_or_constant_name text)
    returns boolean
    as 'session_variable', 'drop' language C security definer;

create function get
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement
    as 'session_variable', 'get' language C security definer cost 2;

create function get_stable
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement stable
    as 'session_variable', 'get' language C security definer cost 2;

create function get_constant
    ( constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement immutable
    as 'session_variable', 'get_constant' language C security definer cost 2;

create function set(variable_name text, new_value anyelement)
    returns boolean 
    as 'session_variable', 'set' language C security definer cost 2;

create function "exists"(variable_name text) 
    returns boolean
    as 'session_variable', 'exists' language C security definer cost 2;
    
create function type_of(variable_or_constant_name text)
    returns regtype
    as 'session_variable', 'type_of' language C security definer cost 2;
    
create function is_constant(variable_or_constant_name text)
    returns boolean
    as 'session_variable', 'is_constant' language C security definer cost 2;

create function init()
    returns integer
    as 'session_variable', 'init' language C security definer;

create function get_session_variable_version()
    returns varchar
    as 'session_variable', 'get_session_variable_version' language C security definer cost 1;

create function is_executing_variable_initialisation()
    returns boolean
    as 'session_variable', 'is_executing_variable_initialisation' language C security definer cost 1;