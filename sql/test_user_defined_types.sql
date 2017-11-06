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
create schema schema_2;
create extension session_variable;
select session_variable.init();

create type schema_1.my_type as 
( text_field text
, int_field int
);
create type schema_2.my_type as 
( int_field int
, float_field float
, varchar_field varchar(25)
);

select session_variable.create_variable('schema_1.my_var', 'schema_1.my_type'::regtype, '("some text", 2)'::schema_1.my_type);
select session_variable.create_variable('schema_2.my_var', 'schema_2.my_type'::regtype, '(223344, 2.5, "text in varchar")'::schema_2.my_type);
select session_variable.create_variable('initially null', 'schema_1.my_type'::regtype);

select session_variable.get('schema_1.my_var', null::schema_1.my_type);
select session_variable.get('schema_2.my_var', null::schema_2.my_type);
select session_variable.get('initially null', null::schema_1.my_type);
select session_variable.get('initially null', null::schema_2.my_type);                      -- fails: wrong type

select session_variable.type_of('schema_1.my_var');
select session_variable.type_of('schema_2.my_var');
select session_variable.type_of('initially null');

select session_variable.set('initially null', session_variable.get('schema_1.my_var', null::schema_1.my_type));
select session_variable.set('schema_1.my_var', null::schema_1.my_type);

select session_variable.get('schema_1.my_var', null::schema_1.my_type);
select session_variable.get('schema_2.my_var', null::schema_2.my_type);
select session_variable.get('initially null', null::schema_1.my_type);

-- cleanup
drop schema if exists session_variable cascade;
drop schema if exists schema_1 cascade;
drop schema if exists schema_2 cascade;