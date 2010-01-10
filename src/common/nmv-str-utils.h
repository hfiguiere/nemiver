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
#ifndef __NMV_STR_UTILS_H__
#define __NMV_STR_UTILS_H__
#include "nmv-ustring.h"

NEMIVER_BEGIN_NAMESPACE (nemiver)
NEMIVER_BEGIN_NAMESPACE (str_utils)

using nemiver::common::UString;

bool
extract_path_and_line_num_from_location (const UString &a_location,
                                         UString &a_file_path,
                                         unsigned &a_line_num);

size_t hexa_to_int (const string &a_hexa_str);
std::string int_to_string (size_t an_int);
vector<UString> split (const UString &a_string, const UString &a_delim);
vector<UString> split_set (const UString &a_string, const UString &a_delim_set);
UString join (const vector<UString> &a_elements,
              const UString &a_delim=" ");
UString join (vector<UString>::const_iterator &a_from,
              vector<UString>::const_iterator &a_to,
              const UString &a_delim=" ");
void chomp (UString &a_string);

UString::size_type get_number_of_lines (const UString &a_string);

UString::size_type get_number_of_words (const UString &a_string);

UString printf (const UString &a_format, ...);

UString vprintf (const UString &a_format, va_list a_args);

NEMIVER_END_NAMESPACE (str_utils)
NEMIVER_END_NAMESPACE (nemiver)

#endif // __NMV_STR_UTILS_H__
