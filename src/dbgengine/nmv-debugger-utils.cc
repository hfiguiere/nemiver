// Author: Dodji Seketeli
/*
 *This file is part of the Nemiver project
 *
 *Nemiver is free software; you can redistribute
 *it and/or modify it under the terms of
 *the GNU General Public License as published by the
 *Free Software Foundation; either version 2,
 *or (at your option) any later version.
 *
 *Nemiver is distributed in the hope that it will
 *be useful, but WITHOUT ANY WARRANTY;
 *without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *See the GNU General Public License for more details.
 *
 *You should have received a copy of the
 *GNU General Public License along with Nemiver;
 *see the file COPYING.
 *If not, write to the Free Software Foundation,
 *Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *See COPYRIGHT file copyright information.
 */
#include <iostream>
#include "nmv-debugger-utils.h"
#include "common/nmv-exception.h"

NEMIVER_BEGIN_NAMESPACE (nemiver)
NEMIVER_BEGIN_NAMESPACE (debugger_utils)

/// \name Definitions of no-op callback slots that can used by client
/// code.
/// 
/// @{
void
null_const_variable_slot (const IDebugger::VariableSafePtr &)
{
}

void
null_const_variable_list_slot (const IDebugger::VariableList &)
{
}

void
null_const_ustring_slot (const UString &)
{
}

void
null_frame_vector_slot (const vector<IDebugger::Frame> &)
{
}

void
null_frame_args_slot (const map<int, IDebugger::VariableList> &)
{
}

void
null_default_slot ()
{
}

void
null_disass_slot (const common::DisassembleInfo &,
                  const std::list<common::Asm> &)
{
}

void
null_breakpoints_slot (const map<string, IDebugger::Breakpoint>&)
{
}
///@}

/// Generate a string of of white spaces.
/// \param a_nb_ws the number of white spaces to generate
/// \param a_ws_str the output string the white spaces are
///  generated into.
void
gen_white_spaces (int a_nb_ws,
                  std::string &a_ws_str)
{
    LOG_FUNCTION_SCOPE_NORMAL_DD;

    for (int i = 0; i < a_nb_ws; i++) {
        a_ws_str += ' ';
    }
}

/// Serialize a variable and its value into a string.
///
/// \param a_var the variable to serialize.
/// \param a_indent_num the number of spaces to indent to before
/// serializing the variable.
/// \param a_out_str the string to serialize the variable into.
void
dump_variable_value (const IDebugger::Variable &a_var,
                     int a_indent_num,
                     std::string &a_out_str)
{
    std::ostringstream os;
    dump_variable_value (a_var, a_indent_num, os, /*print_var_name=*/false);
    a_out_str = os.str ();
}

/// Serialize a variable and its value out to the standard error
/// output stream.
///
/// \param a_var the variable to serialize.
void
dump_variable_value (const IDebugger::Variable &a_var)
{
    dump_variable_value (a_var, 4, std::cerr,
                         /*print_var_name=*/true);
}

/// Load the debugger interface using the default
/// DynamicModuleManager, and initialize it with the configuration
/// manager.
/// 
/// \return the IDebuggerSafePtr
IDebuggerSafePtr
load_debugger_iface_with_confmgr ()
{
    IConfMgrSafePtr confmgr;

    IDebuggerSafePtr debugger = 
        load_iface_and_confmgr<IDebugger> ("gdbengine",
                                           "IDebugger",
                                           confmgr);
    confmgr->register_namespace (/* default nemiver namespace */);
    debugger->do_init (confmgr);
    return debugger;
}

/// Read a string and convert it into the IDebugger::Variable::Format
/// enum.
/// \param a_str the string to consider
/// \return resulting format enum
IDebugger::Variable::Format
string_to_variable_format (const std::string &a_str)
{
    IDebugger::Variable::Format result =
        IDebugger::Variable::UNKNOWN_FORMAT;

    if (a_str == "binary") {
        result = IDebugger::Variable::BINARY_FORMAT;
    } else if (a_str == "decimal") {
        result = IDebugger::Variable::DECIMAL_FORMAT;
    } else if (a_str == "hexadecimal") {
        result = IDebugger::Variable::HEXADECIMAL_FORMAT;
    } else if (a_str == "octal") {
        result = IDebugger::Variable::OCTAL_FORMAT;
    } else if (a_str == "natural") {
        result = IDebugger::Variable::NATURAL_FORMAT;
    }
    return result;
}

/// Serialize an IDebugger::Variable::Format enum into a string.
/// \param a_format the instance of format to serialize.
/// \return the resulting serialization.
std::string
variable_format_to_string (IDebugger::Variable::Format a_format)
{
    std::string result;
    switch (a_format) {
    case IDebugger::Variable::UNDEFINED_FORMAT:
        result = "undefined";
        break;
    case IDebugger::Variable::BINARY_FORMAT:
        result = "binary";
        break;
    case IDebugger::Variable::DECIMAL_FORMAT:
        result = "decimal";
        break;
    case IDebugger::Variable::HEXADECIMAL_FORMAT:
        result = "hexadecimal";
        break;
    case IDebugger::Variable::OCTAL_FORMAT:
        result = "octal";
        break;
    case IDebugger::Variable::NATURAL_FORMAT:
        result = "natural";
        break;
    case IDebugger::Variable::UNKNOWN_FORMAT:
        result = "unknown";
        break;
    default:
        LOG_ERROR ("Invalid format");
        break;
    }
    return result;
}

NEMIVER_END_NAMESPACE (debugger_utils)
NEMIVER_END_NAMESPACE (nemiver)
