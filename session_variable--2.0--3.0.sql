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

create or replace function get_stable
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement stable
    as 'session_variable.so', 'get' language C security definer cost 2;
comment on function get_stable
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    ) is 'Same as get(text, anyelement) but marked as STABLE so the result may be cached';
grant execute on function get_stable
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    to session_variable_user_role;

create or replace function get_constant
    ( constant_name text
    , just_for_result_type anyelement
    )
    returns anyelement immutable
    as 'session_variable.so', 'get_constant' language C security definer cost 2;
comment on function get_constant
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    ) is 'Get the content of a constant. Marked IMMUTABLE so the result may be cached';
grant execute on function get_constant
    ( variable_or_constant_name text
    , just_for_result_type anyelement
    )
    to session_variable_user_role;
