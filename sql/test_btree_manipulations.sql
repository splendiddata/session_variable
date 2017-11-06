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
create extension session_variable;
select session_variable.init();

select session_variable.create_variable('a', 'text'::regtype, 'a'::text);
select session_variable.create_variable('b', 'text'::regtype, 'b'::text);
select session_variable.create_variable('c', 'text'::regtype, 'c'::text);
select session_variable.create_variable('d', 'text'::regtype, 'd'::text);
select session_variable.create_variable('e', 'text'::regtype, 'e'::text);
select session_variable.create_variable('f', 'text'::regtype, 'f'::text);
select session_variable.create_variable('g', 'text'::regtype, 'g'::text);
select session_variable.create_variable('h', 'text'::regtype, 'h'::text);
select session_variable.create_variable('i', 'text'::regtype, 'i'::text);
select session_variable.create_variable('j', 'text'::regtype, 'j'::text);
select session_variable.create_variable('k', 'text'::regtype, 'k'::text);
select session_variable.create_variable('l', 'text'::regtype, 'l'::text);
select session_variable.create_variable('m', 'text'::regtype, 'm'::text);
select session_variable.create_variable('n', 'text'::regtype, 'n'::text);
select session_variable.create_variable('o', 'text'::regtype, 'o'::text);
select session_variable.create_variable('p', 'text'::regtype, 'p'::text);
select session_variable.create_variable('q', 'text'::regtype, 'q'::text);
select session_variable.create_variable('r', 'text'::regtype, 'r'::text);
select session_variable.create_variable('s', 'text'::regtype, 's'::text);
select session_variable.create_variable('t', 'text'::regtype, 't'::text);
select session_variable.create_variable('u', 'text'::regtype, 'u'::text);

select session_variable.init();

select session_variable.get('a', null::text);
select session_variable.get('b', null::text);
select session_variable.get('c', null::text);
select session_variable.get('d', null::text);
select session_variable.get('e', null::text);
select session_variable.get('f', null::text);
select session_variable.get('g', null::text);
select session_variable.get('h', null::text);
select session_variable.get('i', null::text);
select session_variable.get('j', null::text);
select session_variable.get('k', null::text);
select session_variable.get('l', null::text);
select session_variable.get('m', null::text);
select session_variable.get('n', null::text);
select session_variable.get('o', null::text);
select session_variable.get('p', null::text);
select session_variable.get('q', null::text);
select session_variable.get('r', null::text);
select session_variable.get('s', null::text);
select session_variable.get('t', null::text);
select session_variable.get('u', null::text);
select session_variable.get('v', null::text);                                              -- doesn't exist

select session_variable.drop('f');
select session_variable.drop('g');
select session_variable.drop('a');
select session_variable.drop('u');
select session_variable.drop('l');
select session_variable.drop('v');                                                         -- doesn't exist

select session_variable.get('a', null::text);                                              -- doesn't exist
select session_variable.get('b', null::text);
select session_variable.get('c', null::text);
select session_variable.get('d', null::text);
select session_variable.get('e', null::text);
select session_variable.get('f', null::text);                                              -- doesn't exist
select session_variable.get('g', null::text);                                              -- doesn't exist
select session_variable.get('h', null::text);
select session_variable.get('i', null::text);
select session_variable.get('j', null::text);
select session_variable.get('k', null::text);
select session_variable.get('l', null::text);                                              -- doesn't exist
select session_variable.get('m', null::text);
select session_variable.get('n', null::text);
select session_variable.get('o', null::text);
select session_variable.get('p', null::text);
select session_variable.get('q', null::text);
select session_variable.get('r', null::text);
select session_variable.get('s', null::text);
select session_variable.get('t', null::text);
select session_variable.get('u', null::text);                                              -- doesn't exist
select session_variable.get('v', null::text);                                              -- doesn't exist

select session_variable.drop('m');
select session_variable.drop('n');
select session_variable.drop('o');
select session_variable.drop('p');
select session_variable.drop('q');

select session_variable.get('a', null::text);                                              -- doesn't exist
select session_variable.get('b', null::text);
select session_variable.get('c', null::text);
select session_variable.get('d', null::text);
select session_variable.get('e', null::text);
select session_variable.get('f', null::text);                                              -- doesn't exist
select session_variable.get('g', null::text);                                              -- doesn't exist
select session_variable.get('h', null::text);
select session_variable.get('i', null::text);
select session_variable.get('j', null::text);
select session_variable.get('k', null::text);
select session_variable.get('l', null::text);                                              -- doesn't exist
select session_variable.get('m', null::text);                                              -- doesn't exist
select session_variable.get('n', null::text);                                              -- doesn't exist
select session_variable.get('o', null::text);                                              -- doesn't exist
select session_variable.get('p', null::text);                                              -- doesn't exist
select session_variable.get('q', null::text);                                              -- doesn't exist
select session_variable.get('r', null::text);
select session_variable.get('s', null::text);
select session_variable.get('t', null::text);
select session_variable.get('u', null::text);                                              -- doesn't exist
select session_variable.get('v', null::text);                                              -- doesn't exist

select session_variable.create_variable('a', 'text'::regtype, 'a'::text);
select session_variable.create_variable('g', 'text'::regtype, 'g'::text);
select session_variable.create_variable('q', 'text'::regtype, 'q'::text);
select session_variable.create_variable('p', 'text'::regtype, 'p'::text);
select session_variable.create_variable('m', 'text'::regtype, 'm'::text);
select session_variable.create_variable('n', 'text'::regtype, 'n'::text);
select session_variable.create_variable('o', 'text'::regtype, 'o'::text);

select session_variable.get('a', null::text);
select session_variable.get('b', null::text);
select session_variable.get('c', null::text);
select session_variable.get('d', null::text);
select session_variable.get('e', null::text);
select session_variable.get('f', null::text);                                              -- doesn't exist
select session_variable.get('g', null::text);
select session_variable.get('h', null::text);
select session_variable.get('i', null::text);
select session_variable.get('j', null::text);
select session_variable.get('k', null::text);
select session_variable.get('l', null::text);                                              -- doesn't exist
select session_variable.get('m', null::text);
select session_variable.get('n', null::text);
select session_variable.get('o', null::text);
select session_variable.get('p', null::text);
select session_variable.get('q', null::text);
select session_variable.get('r', null::text);
select session_variable.get('s', null::text);
select session_variable.get('t', null::text);
select session_variable.get('u', null::text);                                              -- doesn't exist
select session_variable.get('v', null::text);                                              -- doesn't exist

select session_variable.init();

select session_variable.create_variable('a', 'text'::regtype, 'a'::text);                  -- already exists
select session_variable.create_variable('v', 'text'::regtype, 'v'::text);
select session_variable.create_variable('f', 'text'::regtype, 'f'::text);
select session_variable.create_variable('l', 'text'::regtype, 'l'::text);
select session_variable.create_variable('u', 'text'::regtype, 'u'::text);

select session_variable.get('a', null::text);
select session_variable.get('b', null::text);
select session_variable.get('c', null::text);
select session_variable.get('d', null::text);
select session_variable.get('e', null::text);
select session_variable.get('f', null::text);
select session_variable.get('g', null::text);
select session_variable.get('h', null::text);
select session_variable.get('i', null::text);
select session_variable.get('j', null::text);
select session_variable.get('k', null::text);
select session_variable.get('l', null::text);
select session_variable.get('m', null::text);
select session_variable.get('n', null::text);
select session_variable.get('o', null::text);
select session_variable.get('p', null::text);
select session_variable.get('q', null::text);
select session_variable.get('r', null::text);
select session_variable.get('s', null::text);
select session_variable.get('t', null::text);
select session_variable.get('u', null::text);
select session_variable.get('v', null::text);

select session_variable.init();

select session_variable.drop('g');
select session_variable.drop('h');
select session_variable.drop('p');
select session_variable.drop('k');

select session_variable.get('a', null::text);
select session_variable.get('b', null::text);
select session_variable.get('c', null::text);
select session_variable.get('d', null::text);
select session_variable.get('e', null::text);
select session_variable.get('f', null::text);
select session_variable.get('g', null::text);                                              -- doesn't exist
select session_variable.get('h', null::text);                                              -- doesn't exist
select session_variable.get('i', null::text);
select session_variable.get('j', null::text);
select session_variable.get('k', null::text);                                              -- doesn't exist
select session_variable.get('l', null::text);
select session_variable.get('m', null::text);
select session_variable.get('n', null::text);
select session_variable.get('o', null::text);
select session_variable.get('p', null::text);                                              -- doesn't exist
select session_variable.get('q', null::text);
select session_variable.get('r', null::text);
select session_variable.get('s', null::text);
select session_variable.get('t', null::text);
select session_variable.get('u', null::text);
select session_variable.get('v', null::text);

-- cleanup
drop schema if exists session_variable cascade;