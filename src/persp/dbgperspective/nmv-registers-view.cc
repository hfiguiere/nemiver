//Author: Jonathon Jongsma
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
#include <glib/gi18n.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include "common/nmv-exception.h"
#include "nmv-registers-view.h"
#include "nmv-ui-utils.h"
#include "nmv-i-workbench.h"
#include "nmv-i-perspective.h"

namespace nemiver {

struct RegisterColumns : public Gtk::TreeModelColumnRecord {
    Gtk::TreeModelColumn<IDebugger::register_id_t> id ;
    Gtk::TreeModelColumn<Glib::ustring> name ;
    Gtk::TreeModelColumn<Glib::ustring> value ;

    RegisterColumns ()
    {
        add (id) ;
        add (name) ;
        add (value) ;
    }
};//end Cols

static RegisterColumns&
get_columns ()
{
    static RegisterColumns s_cols ;
    return s_cols ;
}

struct RegistersView::Priv {
public:
    SafePtr<Gtk::TreeView> tree_view ;
    Glib::RefPtr<Gtk::ListStore> list_store ;
    IDebuggerSafePtr& debugger;

    Priv (IDebuggerSafePtr& a_debugger) :
        debugger(a_debugger)
    {
        build_tree_view () ;

        // update breakpoint list when debugger indicates that the list of
        // breakpoints has changed.
        debugger->register_names_listed_signal ().connect (sigc::mem_fun
                (*this, &Priv::on_debugger_registers_listed)) ;
        debugger->changed_registers_listed_signal ().connect (sigc::mem_fun
                (*this, &Priv::on_debugger_changed_registers_listed)) ;

        debugger->stopped_signal ().connect (sigc::mem_fun (*this,
                    &Priv::on_debugger_stopped));
    }

    void build_tree_view ()
    {
        if (tree_view) {return;}
        //create a default tree store and a tree view
        list_store = Gtk::ListStore::create (get_columns ()) ;
        tree_view.reset (new Gtk::TreeView (list_store)) ;

        //create the columns of the tree view
        tree_view->append_column_editable (_("ID"), get_columns ().id) ;
        tree_view->append_column (_("Name"), get_columns ().name) ;
        tree_view->append_column (_("Value"), get_columns ().value) ;
    }

    void on_debugger_stopped (const UString,
                              bool,
                              const IDebugger::Frame &,
                              int,
                              const UString&)
    {
        static bool first_run = true;
        if (first_run)
        {
            first_run = false;
            debugger->list_register_names ();
        }
        else
        {
            debugger->list_changed_registers ();
        }
    }

    void on_debugger_registers_listed
                            (const map<IDebugger::register_id_t, UString> &a_regs,
                             const UString &a_cookie)
    {
        LOG_FUNCTION_SCOPE_NORMAL_DD;
        NEMIVER_TRY
        THROW_IF_FAIL (list_store);
        if (a_cookie.empty ()) {}
        list_store->clear ();
        LOG_DD ("got num registers: " << (int)a_regs.size ());
        for (std::map<IDebugger::register_id_t, UString>::const_iterator reg_iter = a_regs.begin ();
                reg_iter != a_regs.end (); ++reg_iter) {
            Gtk::TreeModel::iterator tree_iter = list_store->append ();
            (*tree_iter)[get_columns ().id] = reg_iter->first;
            (*tree_iter)[get_columns ().name] = reg_iter->second;
            LOG_DD ("got register: " << reg_iter->second);
        }
        NEMIVER_CATCH
    }

    void on_debugger_changed_registers_listed
            (std::list<IDebugger::register_id_t> a_regs,
             const UString &a_cookie)
    {
        LOG_FUNCTION_SCOPE_NORMAL_DD;
        NEMIVER_TRY
        if (a_cookie.empty()) {}
        for (Gtk::TreeModel::iterator tree_iter = list_store->children ().begin ();
                tree_iter != list_store->children ().end (); ++tree_iter) {
            IDebugger::register_id_t id = (*tree_iter)[get_columns ().id];
            if (std::find (a_regs.begin (), a_regs.end (), id) != a_regs.end ())
            {
                (*tree_iter)[get_columns ().value] = "*";
                LOG_DD ("changed register: " << (int)id);
            }
            else
            {
                (*tree_iter)[get_columns ().value] = "";
            }
        }
        NEMIVER_CATCH
    }

};//end class RegistersView::Priv

RegistersView::RegistersView (IDebuggerSafePtr& a_debugger)
{
    m_priv.reset (new Priv (a_debugger));
}

RegistersView::~RegistersView ()
{
    LOG_D ("deleted", "destructor-domain") ;
}

Gtk::Widget&
RegistersView::widget () const
{
    THROW_IF_FAIL (m_priv) ;
    THROW_IF_FAIL (m_priv->tree_view) ;
    THROW_IF_FAIL (m_priv->list_store) ;
    return *m_priv->tree_view ;
}

void
RegistersView::clear ()
{
}

}//end namespace nemiver

