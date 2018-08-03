# session_variable
The session_variable Postgres database extension provides a way to create and
maintain session scoped variables and constants. This extension can be part of
a solution to mimic Oracle's global constants and variables.
<h2>Introduction</h2> 
The session_variable extension registers variables and constants. But internally
they are intermixed and treated as the same. There is just a boolean that
indicates whether or not the session_variable.set(variable_name, value) can be
invoked. So variable names and constant names must be unique within both types.
For the remaining text where variables are mentioned, constants are meant as
well.

Variables (and constants) are defined (created) on the database level. Each user
session will get a local copy of all defined variables on first invocation of
any of the session_variable functions. Invocations of
session_variable.set(variable_name, value) will ONLY alter the content of the
session local copy of the variable. Other sessions will not be affected in any
way - they have their own copy at their disposal.

The session_variable.init() function reloads all defined variables from the
session_variable.variables table. This function will be invoked when a session
starts, and can be invoked at any time. All variables will be reverted to their
initial state.

Variables can be defined using the
session_variable.create_variable(variable_name, variable_type),
session_variable.create_variable(variable_name, variable_type, initial_value)
or session_variable.create_constant(constant_name, constant_type, value)
administrator functions. The initial value can be null - even the value of a
constant (the profit of this is disputable).

The initial value or the constant value can be altered using the
session_variable.alter_value(variable_or_constant_name, value) administrator
function. The administrator who invokes the alter_value() function will see the
altered value immediately, but all existing sessions will remain working with
the old value or the value that they set themselves. Any new session will see
the altered value. Invocation of the session_variable.init() function will make
the altered value available on the session in which it is invoked.

A variable can be removed using the
session_variable.drop(variable_or_constant_name) administrator function. And
here again existing sessions will not notice any change unless they invoke the
session_variable.init() function.

<h3>Example:</h3>

```
-- First create a variable
select session_variable.create_variable('my_variable', 'text'::regtype, 'initial text'::text);

-- Checked if that worked
select session_variable.get('my_variable', null::text);

-- Change the content of the variable<br>
-- Notice that the prior content is returned
select session_variable.set('my_variable', 'changed text'::text);

-- Used in a bit of plpgsql code
do $$<br>
declare<br>
    my_field text;
begin
    my_field := session_variable.get('my_variable', my_field);
    raise notice 'the content of my_field is "%"', my_field;
end
$$ language plpgsql;

-- cleanup
select session_variable.drop('my_variable');
```

<h2>Postgres versions</h2>
The session_variable database extension has been tested on Postgres versions
9.5, 9.6, 10 and 11.
<h2>Installation</h2>
Install as a normal Posrgres database extension:<br>
 - Make sure pg_config points to the right places<br>
 - execute make<br>
 - execute sudo make install installcheck<br>
and then in the Postgres database execute:<br>
 - create extension session_variable;
 
<h2>Functions<h2>

<h3>session_variable.create_variable(variable_name, variable_type)</h3>
The create_variable function creates a new variable with initial value null.

The created variable will be available in the current session and in sessions
that are created after the committed invocation of
session_variable.create_variable(variable_name, variable_type). Existing
sessions do not see the altered situation unless they invoke the
session_variable.init() function.

  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_name</td>
      <td>text</td>
      <td>Name of the variable to be created</td>
    </tr>
    <tr>
      <td>variable_type</td>
      <td>regtype</td>
      <td>The datatype that can be stored in the
        variable</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if ok</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable type must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>2200F</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>23505</td>
      <td>Variable "<i>&lt;variable name&gt;</i>"
        already exists
      </td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.create_variable('my_variable',
      'text'::regtype);</code>
  </p>

  <h3>
    session_variable.create_variable(variable_name, variable_type, initial_value)
  </h3>
  <p>The create_variable function creates a new variable with the specified
    initial value.</p>
  <p>The created variable will be available in the current session and in
    sessions that are created after the committed invocation of
    session_variable.create_variable(variable_name, variable_type,
    initial_value). Existing sessions do not see the altered situation unless
    they invoke the session_variable.init() function.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_name</td>
      <td>text</td>
      <td>Name of the variable to be created</td>
    </tr>
    <tr>
      <td>variable_type</td>
      <td>regtype</td>
      <td>The datatype that can be stored in the
        variable</td>
    </tr>
    <tr>
      <td valign="top">initial_value</td>
      <td valign="top">anyelement</td>
      <td>The initial value that will be loaded
        on session start and to which the variable will be reverted when the
        session_variable.init() function is invoked. <br> <br> The
        value must have the type specified by variable_type.
      </td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if ok</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable type must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>2200F</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22023</td>
      <td>value must be of type <i>&lt;variable_type&gt;</i>,
        but is of type <i>&lt;the actual type&gt;</i></td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>23505</td>
      <td>Variable "<i>&lt;variable_name&gt;</i>"
        already exists
      </td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.create_variable('my_date_variable',
      'date'::regtype, '2015-07-16'::date);</code>
  </p>

  <h3>
    session_variable.create_constant(constant_name, constant_type, value)
  </h3>
  <p>The create_constant function creates a new constant with the specified
    value.</p>
  <p>A constant is just a variable, but it's content cannot be changed by a
    set(variable_name, value) function invocation.</p>
  <p>The created constant will be available in the current session and in
    sessions that are created after the committed invocation of
    session_variable.create_constant(constant_name, constant_type, value).
    Existing sessions do not see the altered situation unless they invoke the
    session_variable.init() function.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>constant_name</td>
      <td>text</td>
      <td>Name of the constant to be created</td>
    </tr>
    <tr>
      <td>constant_type</td>
      <td>regtype</td>
      <td>The datatype that will be stored in
        this constant</td>
    </tr>
    <tr>
      <td valign="top">value</td>
      <td valign="top">anyelement</td>
      <td>The value that will be loaded on
        session start or inocation of the session_variable.init() function. <br>
        <br> The value must have the type specified by constant_type.
      </td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if ok</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>constant name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>constant type must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>2200F</td>
      <td>constant name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22023</td>
      <td>value must be of type <i>&lt;constant_type&gt;</i>,
        but is of type <i>&lt;the actual type&gt;</i></td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>23505</td>
      <td>Variable "<i>&lt;variable_name&gt;</i>"
        already exists
      </td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select
      session_variable.create_constant('my_environment_constant',
      'text'::regtype, 'Production'::text);</code>
  </p>

  <h3>
    session_variable.alter_value(variable_or_constant_name, value)
  </h3>
  <p>Alters the value of the contstant or the initial value of the variable.</p>
  <p>The altered value will be available in the current session and in
    sessions that are created after the committed invocation of
    session_variable.alter_value(variable_or_constant_name, value). Existing
    sessions do not see the altered situation unless they invoke the
    session_variable.init() function.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_or_constant_name</td>
      <td>text</td>
      <td>Name of the variable or constant of
        which the value is to be changed</td>
    </tr>
    <tr>
      <td valign="top">value</td>
      <td valign="top">anyelement</td>
      <td>The value new (initial) value for the
        specified variable or constant <br> <br> The value must have
        the type that was specified when the variable or constant was created.
      </td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if ok</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>02000</td>
      <td>variable or constant "<i>&lt;variable_or_constant_name&gt;</i>"
        does not exist
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable or constant name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>2200F</td>
      <td>variable or constant name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22023</td>
      <td>value must be of type <i>&lt;type&gt;</i>,
        but is of type <i>&lt;the actual type&gt;</i></td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.alter_value('my_environment_constant',
      'Development'::text);</code>
  </p>

  <h3>
    session_variable.drop(variable_or_constant_name)
  </h3>
  <p>Removes the specified constant or variable.</p>
  <p>The constant or variable will be available any more in the current
    session and in sessions that are created after the committed invocation of
    session_variable.drop(variable_or_constant_name). Existing sessions do not
    see the altered situation unless they invoke the session_variable.init()
    function.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_or_constant_name</td>
      <td>text</td>
      <td>Name of the variable or constant to be
        removed</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if ok</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>02000</td>
      <td>variable or constant "<i>&lt;variable_or_constant_name&gt;</i>"
        does not exist
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable or constant name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>2200F</td>
      <td>variable or constant name must be filled</td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.drop('my_environment_constant');</code>
  </p>

  <h3>
    session_variable.init()
  </h3>
  <p>Reloads all variables and constants in the current session</p>
  <p>All variables that have been changed using
    session_variable.set(variable_name, value) invocations will be undone. The
    effect is visible in the current session only. All other sessions are left
    untouched.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">No arguments</th>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>integer</td>
      <td>the number of variabes and constants
        that are loaded</td>
    </tr>
    <tr>
      <th align="left" colspan="3">No exceptions</th>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.init();</code>
  </p>

  <h3>
    session_variable.set(variable_name, value)
  </h3>
  <p>The set function changes the content of a variable.</p>
  <p>The changed content will be visible in the current session only. The
    session_variable.set(variable_name, value) function will no affect any other
    session in any way. Invocation of the session_variable.init() function will
    undo the effect of any previously invoked
    session_variable.set(variable_name, value) function call.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_name</td>
      <td>text</td>
      <td>Name of the variable to update</td>
    </tr>
    <tr>
      <td valign="top">value</td>
      <td valign="top">anyelement</td>
      <td>The new content for the variable. <br>
        <br> The value must have the type specified for the variable.
      </td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if ok</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>02000</td>
      <td>variable "<i>&lt;variable_name&gt;</i>"
        does not exist
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>0A000</td>
      <td>constant "<i>&lt;variable_name&gt;</i>"
        cannot be set
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>2200F</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22023</td>
      <td>value must be of type <i>&lt;variable_type&gt;</i>,
        but is of type <i>&lt;the actual type&gt;</i></td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.set('my_variable', 'a bit of text for
      my variable'::text);</code>
  </p>

  <h3>
    session_variable.get(variable_or_constant_name,
    just_for_result_type)
  </h3>
  <p>Returns the session local content of the named variable or constant.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_or_constant_name</td>
      <td>text</td>
      <td>Name of the variable or constant</td>
    </tr>
    <tr>
      <td valign="top">just_for_result_type</td>
      <td valign="top">anyelement</td>
      <td>In postgres, a function can only return
        anyelement if it has got an anyelement argument. The type of the
        anyelement argument will be the same as the anyelement returntype. So we
        need an argument here with the type of the variable or constant. <br>
        <br> The value must have the type specified for the variable or
        constant.
      </td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>anyelement</td>
      <td>The content of the variable or constant</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>02000</td>
      <td>variable or constant "<i>&lt;variable_or_constant_name&gt;</i>"
        does not exist
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22023</td>
      <td>please invoke as session_variable.get(<i>&lt;variable_or_constant_name&gt;</i>,
        null::<i>&lt;type&gt;</i>)
      </td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.get('my_variable', null::text);</code>
  </p>

  <h3>
    session_variable.exists(variable_or_constant_name)
  </h3>
  <p>Returns the specified variable exists in the local session.</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">Arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>boolean</td>
      <td>true if the variable or constant exists in
        the current session.</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable name must be filled</td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22023</td>
      <td>please invoke as session_variable.exists(<i>&lt;variable_or_constant_name&gt;</i>)
      </td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.exists('my_variable');</code>
  </p>

  <h3>
    session_variable.type_of(variable_or_constant_name)
  </h3>
  <p>Returns the type of the variable or constant</p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_or_constant_name</td>
      <td>text</td>
      <td>Name of the variable or constant</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>regtype</td>
      <td>The type of the specified variable or
        constant</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>02000</td>
      <td>variable or constant "<i>&lt;variable_or_constant_name&gt;</i>"
        does not exist
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable or constant name must be filled</td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.type_of('my_variable');</code>
  </p>

  <h3>
    session_variable.is_constant(variable_or_constant_name)
  </h3>
  <p>
    Returns true if "<i>variable_or_constant_name</i>" happens to be a constant
    or false if it is a session variable
  </p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>variable_or_constant_name</td>
      <td>text</td>
      <td>Name of the variable or constant</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>regtype</td>
      <td>The type of the specified variable or
        constant</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>02000</td>
      <td>variable or constant "<i>&lt;variable_or_constant_name&gt;</i>"
        does not exist
      </td>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>22004</td>
      <td>variable or constant name must be filled</td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.type_of('my_variable');</code>
  </p>

  <h3>
    session_variable.dump(do_truncate)
  </h3>
  <p>
    Generates a 'script' that may be used as backup. 
  </p>
  <p>
    Take care when using PSQL's \copy command. It will double all backslash (\)
    characters.
  </p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">arguments</th>
    </tr>
    <tr>
      <th align="left">name</th>
      <th align="left">type</th>
      <th align="left">description</th>
    </tr>
    <tr>
      <td>do_truncate</td>
      <td>boolean</td>
      <td>Optional argument, default true.<br><br>If true then the first line
        returned will be "truncate table session_variable.variables;". If false
        then the truncate statement will not be returned and all definitions
        will be appended with " where not
        session_variable.exists(<variable_name>)" </td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>setof text</td>
      <td>The lines that together form the script.</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td colspan="3">none</td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.dump();</code>
  </p>

  <h3>
    session_variable.get_session_variable_version()
  </h3>
  <p>
    Returns the code version of the extension, currently '2.0.4'. 
  </p>
  <table class="arguments">
    <tr>
      <th align="left" colspan="3">arguments</th>
    </tr>
    <tr>
      <td colspan="3">none</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Returns</th>
    </tr>
    <tr>
      <td>&nbsp;</td>
      <td>text</td>
      <td>The code version of the session_variable
       extension.</td>
    </tr>
    <tr>
      <th align="left" colspan="3">Exceptions</th>
    </tr>
    <tr>
      <td colspan="3">none</td>
    </tr>
  </table>
  <p>
    Example:<br>
    <code>select session_variable.get_session_variable_version();</code>
  </p>

<h2>Security</h2>
<p>
Usage of session_variable.create_variable(variable_name, variable_type),
session_variable.create_variable(variable_name, variable_type, initial_value),
session_variable.create_constant(constant_name, constant_type, value),
session_variable.alter_value(variable_or_constant_name, value),
session_variable.drop(variable_or_constant_name) and 
session_variable.dump() is protected by the
"session_variable_administrator_role". 
</p><p>
The remaining functions are protected by the "session_variable_user_role".
</p><p>
The "session_variable_administrator_role" includes the 
"session_variable_user_role".
</p>

## Save / restore

Session variables are stored in the session_variable.variables table, which 
can be saved and restored as any other table. Restored values will be visible
to all sessions that started after the restore committed. Sessions that were
started before the restore will still see the old (session local!) content
unless they invoke session_variable.init().

## Release notes
### version 2
In version 1, the initial values of variables and constants were stored in the
session_variable.variables table in a bytea representing a memory image of the
content. This appeared problematic when copying data from one database to
another as in array types and composite types oids are present in the memory
image. So in version 2 the initial_value column is altered to a text column and
serialization and deserialization is now routed via the typinput and typoutput
functions. So now a database dump is portable (provided that the receiving
database has got all user defined types available).

Returning the previous value in the set() function and the alter() function
proved not very useful and did impose some overhead. So in version 2 these
functions return just a boolean, which will be 'true' in all cases.
#### upgrade to version 2
On the command line:

> git pull<br>
> make clean<br>
> make<br>
> sudo make install

Then in a new database session

> alter extension session_variable update;

Ps.<br>
The database will keep using the version 1 implementation of the extension
until the "alter extension session_variable update;" command is executed. Make
sure you do not restore any dump of the session_variable.variables table that
was created before the "alter extension session_variable update;" into a
database that already executes version 2.

