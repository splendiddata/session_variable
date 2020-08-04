# Copyright (c) Splendid Data Product Development B.V. 2013
# 
# This program is free software: You may redistribute and/or modify under the 
# terms of the GNU General Public License as published by the Free Software 
# Foundation, either version 3 of the License, or (at Client's option) any 
# later version.
# 
# This program is distributed in the hope that it will be useful, but WITHOUT 
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with 
# this program.  If not, Client should obtain one via www.gnu.org/licenses/.
#

MODULE_big = session_variable
OBJS = session_variable.o
EXTENSION = session_variable
DATA = session_variable--3.1.sql session_variable--3.0--3.1.sql \
       session_variable--3.0.sql session_variable--2.0--3.0.sql \
       session_variable--2.0.sql session_variable--1.0--2.0.sql \
       session_variable--1.0.sql
DOCS = session_variable.html session_variable.css
PG_CONFIG = pg_config
REGRESS = test_session_variables test_user_defined_types test_btree_manipulations test_upgrade_1.0_2.0 test_upgrade_2.0_3.0 test_upgrade_3.0_3.1

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
