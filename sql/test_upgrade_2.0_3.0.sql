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

-- init
create schema schema_1;
create extension session_variable version '2.0';
select session_variable.init();

create type schema_1.my_type as 
( text_field text
, int_field int
);
create domain schema_1.my_domain as schema_1.my_type[];

select session_variable.create_variable('schema_1.my_var', 'schema_1.my_type'::regtype, '("some text", 2)'::schema_1.my_type);
select session_variable.create_variable('schema_1.my_var[]', 'schema_1.my_type[]'::regtype, array['("element1", 1)'::schema_1.my_type,'("element2", 2)'::schema_1.my_type]);
select session_variable.create_variable('initially null', 'schema_1.my_type'::regtype);
select session_variable.create_variable('schema_1.my_domain', 'schema_1.my_domain'::regtype, array['("element 1", 123)'::schema_1.my_type,'("element 2,2", 234)'::schema_1.my_type]::schema_1.my_domain);
select session_variable.create_variable('int[]', 'int[]'::regtype, array[[1,2],[3,4],[5,5]]);
select session_variable.create_variable('just_text', 'text'::regtype, 'just some text'::text);
select session_variable.create_variable('just_int', 'int'::regtype, 0);
select session_variable.create_variable('just_smallint', 'smallint'::regtype, 123);
select session_variable.create_constant('const_boolean', 'boolean'::regtype, true);

select session_variable.get('schema_1.my_var', null::schema_1.my_type);
select session_variable.get('schema_1.my_var[]', null::schema_1.my_type[]);
select session_variable.get('initially null', null::schema_1.my_type);
select session_variable.get('schema_1.my_domain', null::schema_1.my_domain);
select session_variable.get('int[]', null::int[]);
select session_variable.get('just_text', null::varchar);
select session_variable.get('just_int', 15);
select session_variable.get('just_smallint', null::smallint);

select session_variable.get('const_boolean', null::boolean);
select session_variable.get_stable('const_boolean', null::boolean);           -- should fail as version 2.0 does not have function get_stable()  
select session_variable.get_constant('const_boolean', null::boolean);         -- should fail as version 2.0 does not have function get_constant()

select session_variable.init();

select session_variable.get('schema_1.my_var', null::schema_1.my_type);
select session_variable.get('schema_1.my_var[]', null::schema_1.my_type[]);
select session_variable.get('initially null', null::schema_1.my_type);
select session_variable.get('schema_1.my_domain', null::schema_1.my_domain);
select session_variable.get('int[]', null::int[]);
select session_variable.get('just_text', null::varchar);
select session_variable.get('just_int', 15);
select session_variable.get('just_smallint', null::smallint);

select session_variable.set('int[]', array[1,2,3]);

select session_variable.dump();

alter extension session_variable update;

select session_variable.get('schema_1.my_var', null::schema_1.my_type);
select session_variable.get('schema_1.my_var[]', null::schema_1.my_type[]);
select session_variable.get('initially null', null::schema_1.my_type);
select session_variable.get('schema_1.my_domain', null::schema_1.my_domain);
select session_variable.get('int[]', null::int[]);
select session_variable.get('just_text', null::varchar);
select session_variable.get('just_int', 15);
select session_variable.get('just_smallint', null::smallint);

select session_variable.get('const_boolean', null::boolean);
select session_variable.get_stable('const_boolean', null::boolean);  
select session_variable.get_constant('const_boolean', null::boolean);  
select session_variable.get_constant('just_smallint', null::smallint);        -- should fail just_smallint is not a constant

select variable_name, initial_value from session_variable.variables order by variable_name;

select session_variable.dump();

select session_variable.set('int[]', array[1,2,3]);

-- cleanup
drop schema if exists session_variable cascade;
drop schema if exists schema_1 cascade;
