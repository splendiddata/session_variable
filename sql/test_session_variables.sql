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

select session_variable.get_session_variable_version();

select session_variable.get('does not exist', null::text);                                 -- fails: no variables exist

select session_variable.create_variable('some_date', 'date'::regtype, '2015-07-15'::date);
select session_variable.create_variable('an integer', 'integer'::regtype, 123456789);
select session_variable.create_variable('integer_initially_zero', 'integer'::regtype, 0);
select session_variable.create_variable('just text', 'text'::regtype, 'just some text'::text);
select session_variable.create_variable('varchar', 'varchar'::regtype, 'a varchar text'::varchar);
select session_variable.create_constant('numeric const', 'numeric'::regtype, 123.45);
select session_variable.create_variable('initially null', 'text'::regtype, null::text);
select session_variable.create_variable('a_record', 'record'::regtype, ('some text', '2015-07-17'::date, 123.45::numeric));  -- fails: pseudo type
select session_variable.create_variable('int[][]', 'int[][]'::regtype, '{{1,2},{3,4},{5,6}}'::int[][]);

select session_variable.create_variable('wrongdate', 'interval'::regtype, '2015-07-15'::date); -- fails: wrong datatype
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
select session_variable.get('numeric const', null::date);                                  -- fails: wrong type
select session_variable.get('int[][]', null::int[][]);
select session_variable.get('initially null', null::text);
select session_variable.get('a_record', null::record);                                     -- fails: not created because of pseudo type


select session_variable.exists('int[][]');
select session_variable.exists('initially null');
select session_variable.exists('does not exist');

select session_variable.drop('int[][]');

select session_variable.set('some_date', '2050-12-11'::date);
select session_variable.set('an integer', 987654321);
select session_variable.set('just text', 'a new bit of text'::text);
select session_variable.set('varchar', 'an altered varchar text'::varchar);
select session_variable.set('initially null', 'now filled'::text);

select session_variable.set('numeric const', 223344);                                      -- fails: constants cannot be set
select session_variable.set('does not exist', null::text);                                 -- fails: does not exist

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);
select session_variable.set('initially null', null::text);

select session_variable.init();

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('integer_initially_zero', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.set('initially null', null::text);
select session_variable.get('numeric const', null::numeric);

select session_variable.alter_value('some_date', '1900-01-01'::date);
select session_variable.alter_value('an integer', 0);
select session_variable.alter_value('just text', ''::text);
select session_variable.alter_value('varchar', null::varchar);
select session_variable.alter_value('numeric const', 3333333333333333333333.3333333333333);

select session_variable.alter_value('does not exist', null::text);                         -- fails: does not exist
select session_variable.alter_value('an integer', null::date);                             -- fails: wrong datatype

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');
select session_variable.get('an integer', null::integer);
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);

select session_variable.set('an integer', 123.456::float);
select session_variable.get('an integer', null::smallint);

select session_variable.set('varchar', '2018-03-15'::date);
select session_variable.get('varchar', null::char);
select session_variable.set('varchar', '12345'::text);
select session_variable.set('varchar', 23.456::numeric);                                   -- fails: the result cannot be cast from varchar to numeric

select session_variable.drop('some_date');
select session_variable.drop('an integer');

select session_variable.dump();
select session_variable.dump(false);

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');               -- fails: has just been dropped
select session_variable.get('an integer', null::integer);                                  -- fails: has just been dropped
select session_variable.get('just text', null::text);
select session_variable.get('varchar', null::varchar);
select session_variable.get('numeric const', null::numeric);

create or replace function session_variable.variable_initialisation()
   returns void language plpgsql as $$
begin
	if session_variable.is_executing_variable_initialisation()
	then
	    raise notice 'executing session_variable.variable_initialisation() on behalf of session_variable initialisation code';
	    select session_variable.set('numeric const', 223344);                              -- setting a constant should succeed now  
	else
	    raise notice 'executing session_variable.variable_initialisation() on its own account';
	end if;
end;
$$; 
select session_variable.get('numeric const', null::numeric);
select session_variable.variable_initialisation();
select session_variable.get('numeric const', null::numeric);
select session_variable.init();
select session_variable.get('numeric const', null::numeric);

drop function session_variable.variable_initialisation();
select session_variable.init();
select session_variable.get('numeric const', null::numeric);

select session_variable.drop('just text');
select session_variable.drop('varchar');
select session_variable.drop('numeric const');
select session_variable.drop('initially null');
select session_variable.drop('does not exist');                                            -- fails: does not exist

select to_char(session_variable.get('some_date', null::date), 'yyyy-mm-dd');               -- fails: has just been dropped
select session_variable.get('an integer', null::integer);                                  -- fails: has just been dropped
select session_variable.get('just text', null::text);                                      -- fails: has just been dropped
select session_variable.get('varchar', null::varchar);                                     -- fails: has just been dropped
select session_variable.get('numeric const', null::numeric);                               -- fails: has just been dropped

-- cleanup
drop schema if exists session_variable cascade;