//Author: Dodji Seketeli
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
 *GNU General Public License along with Goupil;
 *see the file COPYING.
 *If not, write to the Free Software Foundation,
 *Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *See COPYRIGHT file copyright information.
 */

#include <vector>
#include <iostream>
#include <glib/gi18n.h>
#include <libglademm.h>
#include <gtkmm/dialog.h>
#include <gtkmm/entry.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/stock.h>
#include "nmv-exception.h"
#include "nmv-run-program-dialog.h"
#include "nmv-env.h"
#include "nmv-ustring.h"

using namespace std ;
using namespace nemiver::common ;

namespace nemiver {

void RunProgramDialog::on_add_new_variable()
{
    // This will add a new blank row the the treeview for now.  Then the user
    // can click in the new row and edit the cells to define environment
    // variables.  There will be a better way to do this in the future, but I'm
    // not sure how it will be done yet.
    m_model->append();
}

RunProgramDialog::RunProgramDialog (const UString &a_root_path) :
    Dialog(a_root_path, "runprogramdialog.glade", "runprogramdialog"),
    m_model(Gtk::ListStore::create(m_env_columns))
{
    THROW_IF_FAIL (glade) ;
    working_directory (Glib::get_current_dir ()) ;
    m_treeview_environment = env::get_widget_from_glade<Gtk::TreeView> (glade,
            "treeview_environment");
    m_treeview_environment->set_model (m_model);
    m_treeview_environment->append_column_editable ("Variable", m_env_columns.varname);
    m_treeview_environment->append_column_editable ("Value", m_env_columns.value);

    Gtk::Button* button = env::get_widget_from_glade<Gtk::Button> (glade,
            "button_new_var");
    THROW_IF_FAIL (button) ;
    button->signal_clicked().connect(sigc::mem_fun(*this, &RunProgramDialog::on_add_new_variable));
}

RunProgramDialog::~RunProgramDialog ()
{
}

UString
RunProgramDialog::program_name () const
{
    THROW_IF_FAIL (glade) ;
    Gtk::FileChooserButton *chooser = env::get_widget_from_glade<Gtk::FileChooserButton> (glade,
                                                                "filechooserbutton_program") ;
    return chooser->get_filename () ;
}

void
RunProgramDialog::program_name (const UString &a_name)
{
    THROW_IF_FAIL (glade) ;
    Gtk::FileChooserButton *chooser = env::get_widget_from_glade<Gtk::FileChooserButton> (glade,
                                                                "filechooserbutton_program") ;
    chooser->set_filename (a_name) ;
}

UString
RunProgramDialog::arguments () const
{
    THROW_IF_FAIL (glade) ;
    Gtk::Entry *entry = env::get_widget_from_glade<Gtk::Entry> (glade,
                                                                "argumentsentry");
    return entry->get_text () ;
}

void
RunProgramDialog::arguments (const UString &a_args)
{
    THROW_IF_FAIL (glade) ;
    Gtk::Entry *entry = env::get_widget_from_glade<Gtk::Entry> (glade,
                                                                "argumentsentry");
    entry->set_text (a_args) ;
}

UString
RunProgramDialog::working_directory () const
{
    THROW_IF_FAIL (glade) ;
    Gtk::FileChooserButton *chooser =
        env::get_widget_from_glade<Gtk::FileChooserButton> (glade,
                                                "filechooserbutton_workingdir");
    return chooser->get_filename () ;
}

void
RunProgramDialog::working_directory (const UString &a_dir)
{
    THROW_IF_FAIL (glade) ;
    Gtk::FileChooserButton *chooser =
        env::get_widget_from_glade<Gtk::FileChooserButton> (glade,
                                                "filechooserbutton_workingdir");
    chooser->set_filename (a_dir) ;
}

map<UString, UString>
RunProgramDialog::environment_variables () const
{
    THROW_IF_FAIL (m_treeview_environment) ;
    map<UString, UString> env_vars;
    for (Gtk::TreeModel::iterator iter = m_model->children().begin();
            iter != m_model->children().end(); ++iter)
    {
        // for some reason I have to explicitly convert from Glib::ustring to
        // UString here or it won't compile
        env_vars[UString((*iter)[m_env_columns.varname])] =
            UString((*iter)[m_env_columns.value]);
    }
    return env_vars;
}

void
RunProgramDialog::environment_variables (const map<UString, UString> &vars)
{
    THROW_IF_FAIL (m_treeview_environment) ;
    // clear out the old data so we can set the new data
    m_model->clear();
    for (map<UString, UString>::const_iterator iter = vars.begin();
            iter != vars.end(); ++iter)
    {
        Gtk::TreeModel::iterator treeiter = m_model->append();
        (*treeiter)[m_env_columns.varname] = iter->first;
        (*treeiter)[m_env_columns.value] = iter->second;
    }
}

}//end namespace nemiver

