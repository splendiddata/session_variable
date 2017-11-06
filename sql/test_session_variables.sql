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

-- set client_min_messages = DEBUG4;

-- init
create extension session_variable;
select session_variable.init();

select session_variable.get('does not exist', null::text);                                 -- fails: no variables exist

select session_variable.create_variable('some_date', 'date'::regtype, '2015-07-15'::date);
select session_variable.create_variable('an integer', 'integer'::regtype, 123456789);
select session_variable.create_variable('just text', 'text'::regtype, 'just some text'::text);
select session_variable.create_variable('varchar', 'varchar'::regtype, 'a varchar text'::varchar);
select session_variable.create_constant('numeric const', 'numeric'::regtype, 123.45);
select session_variable.create_variable('initially null', 'text'::regtype, null::text);
select session_variable.create_variable('a_record', 'record'::regtype, ('some text', '2015-07-17'::date, 123.45::numeric));
select session_variable.create_variable('int[][]', 'int[][]'::regtype, '{{1,2},{3,4},{5,6}}'::int[][]);

select session_variable.create_variable('wrongdate', 'text'::regtype, '2015-07-15'::date); -- fails: wrong datatype
select session_variable.get('does not exist', null::text);                                 -- fails: does not exist

select session_variable.type_of('some_date');
select session_variable.type_of('an integer');
select session_variable.type_of('varchar');
select session_variable.type_of('numeric const');
select session_variable.type_of('int[][]');
select session_variable.type_of('does not exist');                                         -- fails: does not exist
select session_variable.type_of(null);                                                     -- fails: variable_or_constan_name must be filled

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);
select session_variable.get('numeric const', null::text);                                  -- fails: wrong type
select session_variable.get('int[][]', null::int[][]);
select session_variable.get('initially null', null::text);
select session_variable.get('a_record', null::record);


select session_variable.exists('int[][]');
select session_variable.exists('initially null');
select session_variable.exists('does not exist');

select session_variable.drop('int[][]');

select session_variable.set('some_date', '2050-12-11'::date);
select session_variable.set('an integer', 987654321);
select session_variable.set('just text', 'a new bit of text'::text);
select session_variable.set('varchar', 'an altered varchar text'::varchar);
select session_variable.set('initially null', 'now filled'::text);
select session_variable.set('a_record', null::record);

select session_variable.set('numeric const', 223344);                                      -- fails: constants cannot be set
select session_variable.set('does not exist', null::text);                                 -- fails: does not exist

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);
select session_variable.set('initially null', null::text);
select session_variable.get('a_record', null::record);

select session_variable.init();

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.set('initially null', null::text);
select session_variable.get('numeric const', null::numeric);

select session_variable.alter_value('some_date', '1900-01-01'::date);
select session_variable.alter_value('an integer', 0);
select session_variable.alter_value('just text', ''::text);
select session_variable.alter_value('varchar', null::varchar);
select session_variable.alter_value('numeric const', 3333333333333333333333.3333333333333);
select session_variable.alter_value('a_record', (123, 456, 'altered text'));

select session_variable.alter_value('does not exist', null::text);                         -- fails: does not exist
select session_variable.alter_value('an integer', null::text);                             -- fails: wrong datatype

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);
select session_variable.get('a_record', null::record);

select session_variable.drop('some_date');
select session_variable.drop('an integer');

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');               -- fails: has just been dropped
select session_variable.get('an integer', null::integer);                                  -- fails: has just been dropped
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);
select session_variable.get('a_record', null::record);

select session_variable.drop('just text');
select session_variable.drop('varchar');
select session_variable.drop('numeric const');
select session_variable.drop('initially null');
select session_variable.drop('a_record');
select session_variable.drop('does not exist');                                            -- fails: does not exist

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');               -- fails: has just been dropped
select session_variable.get('an integer', null::integer);                                  -- fails: has just been dropped
select session_variable.get('just text', null::text);                                      -- fails: has just been dropped
select session_variable.get('varchar', null::varchar);                                     -- fails: has just been dropped
select session_variable.get('numeric const', null::numeric);                               -- fails: has just been dropped
select session_variable.get('a_record', null::record);                                     -- fails: has just been dropped

-- cleanup
drop schema if exists session_variable cascade;