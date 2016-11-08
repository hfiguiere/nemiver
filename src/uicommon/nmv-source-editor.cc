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
 *GNU General Public License along with Nemiver;
 *see the file COPYING.
 *If not, write to the Free Software Foundation,
 *Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *See COPYRIGHT file copyright information.
 */
#include "config.h"
#include <map>
#include <glib/gi18n.h>
#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtksourceviewmm/mark.h>
#include <gtksourceviewmm/markattributes.h>
#include <gtksourceviewmm.h>
#include <giomm/file.h>
#include <giomm/contenttype.h>
#include "common/nmv-exception.h"
#include "common/nmv-sequence.h"
#include "common/nmv-str-utils.h"
#include "common/nmv-asm-utils.h"
#include "uicommon/nmv-ui-utils.h"
#include "nmv-source-editor.h"

using namespace std;
using namespace nemiver::common;
using Gsv::Mark;
using Gsv::Language;
using Gsv::LanguageManager;

NEMIVER_BEGIN_NAMESPACE (nemiver)

const char* BREAKPOINT_ENABLED_CATEGORY = "breakpoint-enabled-category";
const char* BREAKPOINT_DISABLED_CATEGORY = "breakpoint-disabled-category";
const char* COUNTPOINT_CATEGORY;
const char* WHERE_CATEGORY = "line-pointer-category";

const char* WHERE_MARK = "where-marker";

void
on_line_mark_activated_signal (GtkSourceView *a_view,
                               GtkTextIter *a_iter,
                               GdkEvent *a_event,
                               gpointer a_pointer);

class SourceView : public Gsv::View
{

    sigc::signal<void, int, bool> m_marker_region_got_clicked_signal;
    friend void on_line_mark_activated_signal (GtkSourceView *a_view,
                                               GtkTextIter *a_iter,
                                               GdkEvent *a_event,
                                               gpointer a_pointer);

public:
    SourceView (Glib::RefPtr<Buffer> &a_buf) :
        Gsv::View (a_buf)
    {
        init_font ();
        enable_events ();
        g_signal_connect (gobj (),
                          "line-mark-activated",
                          G_CALLBACK (&on_line_mark_activated_signal),
                          this);
    }

    SourceView () :
        Gsv::View ()
    {
        init_font ();
    }

    void
    init_font ()
    {
        Pango::FontDescription font("monospace");
        override_font(font);
    }

    void
    enable_events ()
    {
        add_events (Gdk::LEAVE_NOTIFY_MASK
                    |Gdk::BUTTON_PRESS_MASK);
    }

    bool
    on_button_press_event (GdkEventButton *a_event)
    {
        if (a_event->type == GDK_BUTTON_PRESS
            && a_event->button == 3) {
            // The right button has been pressed. Get out.
            return false;
        } else {
            Gtk::Widget::on_button_press_event (a_event);
            return false;
        }
    }

    sigc::signal<void, int, bool>&
    marker_region_got_clicked_signal ()
    {
        return m_marker_region_got_clicked_signal;
    }

    /// Given a menu to popup, pop it up at the right place;
    ///
    /// \param a_event the event that triggered this whole button
    /// popup business or NULL.
    ///
    /// \param a_attach_to a widget to attach the popup menu to, or NULL.
    ///
    /// \param a_menu the menu to popup.
    void
    setup_and_popup_menu (GdkEventButton* a_event,
                          Gtk::Widget *a_attach_to,
                          Gtk::Menu *a_menu)
    {
        Gtk::TextBuffer::iterator cur_iter;

        RETURN_IF_FAIL (a_menu);

        if (a_attach_to && a_menu->get_attach_widget () == NULL) {
            gtk_menu_attach_to_widget (GTK_MENU (a_menu->gobj()),
                                       GTK_WIDGET (a_attach_to->gobj()),
                                       NULL);
        }

        Gtk::TextIter start, end;
        Glib::RefPtr<Gsv::Buffer> buffer = get_source_buffer ();
        THROW_IF_FAIL (buffer);

        a_menu->popup (a_event ? a_event->button : 0,
                       a_event ? a_event->time : 0);
    }
};//end class Sourceview

void
on_line_mark_activated_signal (GtkSourceView *a_view,
                               GtkTextIter *a_iter,
                               GdkEvent *a_event,
                               gpointer a_pointer)
{
    RETURN_IF_FAIL (a_view && a_iter && a_event && a_pointer);

    SourceView *sv = static_cast<SourceView*> (a_pointer);

    if (a_event->type == GDK_BUTTON_PRESS
        && ((GdkEventButton*)a_event)->button == 1)
        sv->marker_region_got_clicked_signal ().emit
            (gtk_text_iter_get_line (a_iter) + 1,
             false/*No dialog requested*/);
}

struct SourceEditor::Priv {
    Sequence sequence;
    UString root_dir;
    Gtk::Window &parent_window;
    nemiver::SourceView *source_view;
    Gtk::Label *line_col_label;
    Gtk::Box *status_box;
    enum SourceEditor::BufferType buffer_type;
    UString path_;

    struct NonAssemblyBufContext {
        Glib::RefPtr<Buffer> buffer;
        std::map<int, Glib::RefPtr<Gsv::Mark> > markers;
        int current_column;
        int current_line;
        sigc::signal<void, int, int> signal_insertion_moved;
        sigc::signal<void, int, bool> marker_region_got_clicked_signal;

        NonAssemblyBufContext (Glib::RefPtr<Buffer> a_buf,
                                int a_cur_col, int a_cur_line) :
            buffer (a_buf),
            current_column (a_cur_col),
            current_line (a_cur_line)
        {
        }

        NonAssemblyBufContext (int a_cur_col, int a_cur_line) :
            current_column (a_cur_col),
            current_line (a_cur_line)
        {
        }

        NonAssemblyBufContext () :
            current_column (-1),
            current_line (-1)
        {
        }
    } non_asm_ctxt;

    struct AssemblyBufContext {
        Glib::RefPtr<Buffer> buffer;
        std::map<int, Glib::RefPtr<Gsv::Mark> > markers;
        int current_line;
        int current_column;
        Address current_address;

        AssemblyBufContext () :
            current_line (-1),
            current_column (-1)
        {
        }

        AssemblyBufContext
                    (Glib::RefPtr<Buffer> a_buf) :
            buffer (a_buf),
            current_line (-1),
            current_column (-1)
        {
        }
    } asm_ctxt;

    sigc::signal<void, const Gtk::TextBuffer::iterator&>
                                                    insertion_changed_signal;


    void
    register_assembly_source_buffer (Glib::RefPtr<Buffer> &a_buf)
    {
        asm_ctxt.buffer = a_buf;
        source_view->set_source_buffer (a_buf);
        init_assembly_buffer_signals ();
    }

    void
    register_non_assembly_source_buffer (Glib::RefPtr<Buffer> &a_buf)
    {
        non_asm_ctxt.buffer = a_buf;
        source_view->set_source_buffer (a_buf);
        init_non_assembly_buffer_signals ();
    }

    bool
    switch_to_assembly_source_buffer ()
    {
        RETURN_VAL_IF_FAIL (source_view, false);

        if (asm_ctxt.buffer) {
            if (source_view->get_source_buffer ()
                != asm_ctxt.buffer)
                source_view->set_source_buffer (asm_ctxt.buffer);
            return true;
        }
        return false;
    }

    bool
    switch_to_non_assembly_source_buffer ()
    {
        RETURN_VAL_IF_FAIL (source_view, false);

        if (asm_ctxt.buffer) {
            if (source_view->get_source_buffer ()
                != non_asm_ctxt.buffer) {
                source_view->set_source_buffer (non_asm_ctxt.buffer);
                // Reset the "current line" that was selected previously.
                non_asm_ctxt.current_line = -1;
            }
            return true;
        }
        return false;
    }

    SourceEditor::BufferType
    get_buffer_type () const
    {
        Glib::RefPtr<Buffer> buf = source_view->get_source_buffer ();
        if (buf == non_asm_ctxt.buffer)
            return BUFFER_TYPE_SOURCE;
        else if (buf == asm_ctxt.buffer)
            return BUFFER_TYPE_ASSEMBLY;
        else
            return BUFFER_TYPE_UNDEFINED;
    }

    std::map<int, Glib::RefPtr<Gsv::Mark> >*
    get_markers ()
    {
        SourceEditor::BufferType t = get_buffer_type ();
        switch (t) {
            case SourceEditor::BUFFER_TYPE_SOURCE:
                return &non_asm_ctxt.markers;
            case SourceEditor::BUFFER_TYPE_ASSEMBLY:
                return &asm_ctxt.markers;
            case SourceEditor::BUFFER_TYPE_UNDEFINED:
                return 0;
            default:
                LOG_ERROR ("Invalid buffer type.");
        }
        return 0;
    }

    typedef std::pair<Address, size_t> AddrLine;
    typedef std::pair<AddrLine, AddrLine> AddrLineRange;

    /// Return the smallest range of address/line pair enclosing the
    /// instruction whose address is an_addr.
    /// \param a_buf the buffer to look into.
    /// \param an_addr the address of the instruction to look for.
    /// \param a_range output parameter. the range found.
    /// This range either encloses
    /// an_addr if such a range is found, or is the range (possibly
    /// reduced to a single value) that comes after or before an_addr
    /// if no enclosing range is found in the buffer for an_addr.
    /// \return Range::VALUE_SEARCH_RESULT_EXACT If an instruction which
    /// address is an_addr is found; Range::VALUE_SEARCH_RESULT_BEFORE
    /// if an_addr is smaller than all the addresses of a_buf. In
    /// that case, the smaller address of a_buf is returned into
    /// a_range; Range::VALUE_SEARCH_RESULT_AFTER if an_addr is higher
    /// than all addresses found in the buffer. In that case the
    /// higher address found in a_buf is returned into a_range;
    /// Range::VALUE_SEARCH_RESULT_NONE when no address has been found
    /// in the buffer.
    common::Range::ValueSearchResult
    get_smallest_range_containing_address (Glib::RefPtr<Buffer> a_buf,
                                           const Address &an_addr,
                                           AddrLineRange &a_range) const
                                    
    {
        Gtk::TextBuffer::iterator it = a_buf->begin ();
        size_t  i;
        std::string addr;
        AddrLine lower_bound, upper_bound;

        THROW_IF_FAIL (it.starts_line ());

        while (!it.is_end ()) {
            // We must always be at the beginning of a line here.
            THROW_IF_FAIL (it.starts_line ());
            addr.clear ();
            for (i = 0;
                 !isspace (it.get_char ()) && it.ends_line () != true
                     && i < an_addr.string_size ();
                 ++it, ++i) {
                addr += it.get_char ();
            }

            int match = (addr.compare (an_addr.to_string ()));

            if (match < 0
                && str_utils::string_is_hexa_number (addr)) {
                lower_bound.first = addr;
                lower_bound.second = it.get_line () + 1;
            }

            if (match > 0
                && str_utils::string_is_hexa_number (addr)) {
                if (lower_bound.first.empty ()) {
                    // So we are seing an @ that is greater than
                    // an_addr without having ever seen an @ that is
                    // lower than an_addr.  That means all the @s of
                    // the buffer are greater than an_addr.
                    a_range.first.first = addr;
                    a_range.first.second = it.get_line () + 1;
                    a_range.second = a_range.first;
                    return common::Range::VALUE_SEARCH_RESULT_BEFORE;
                } else {
                    // So the previous @ we saw was lower than
                    // a_address and this one is greater. it means we
                    // the buffer does not contain a_address, but
                    // rather contains at a range of @s that surrounds
                    // a_address. Return that range.
                    upper_bound.first = addr;
                    upper_bound.second = it.get_line () + 1;
                    a_range.first = lower_bound;
                    a_range.second = upper_bound;
                    return common::Range::VALUE_SEARCH_RESULT_WITHIN;
                }
            }

            if (match == 0) {
                a_range.first.first = an_addr;
                a_range.first.second = it.get_line () + 1;
                a_range.second = a_range.first;
                return common::Range::VALUE_SEARCH_RESULT_EXACT;
            } else {
                // Go to next line.
                it.forward_line ();
            }
        }

        if (!lower_bound.first.empty ()) {
            if (upper_bound.first.empty ()) {
                a_range.first.first = lower_bound.first;
                a_range.second = a_range.first;
                return common::Range::VALUE_SEARCH_RESULT_AFTER;
            } else {
                THROW ("unreachable");
            }
        }
         
        return common::Range::VALUE_SEARCH_RESULT_NONE;
    }

    /// Return the number of the line in a_buf that contains an asm
    /// instruction of whose address is an_addr.
    /// \param a_buf the buffer to search into
    /// \param an_addr the address of the instruction to look for.
    /// \param a_approximate if true and if an_addr hasn't been found
    /// in the buffer, return the line that matches the closes address
    /// found.
    /// \param a_line output parameter. Is set to the line found, iff
    /// the function returns true. Otherwise this parameter is not touched.
    /// \return true upon successful completion, false otherwise.
    bool
    address_2_line (Glib::RefPtr<Buffer> a_buf,
		    const Address an_addr,
                    bool a_approximate,
		    int &a_line) const
    {
        if (!a_buf)
            return false;

        AddrLineRange range;
        common::Range::ValueSearchResult s =
            get_smallest_range_containing_address (a_buf, an_addr, range);

        if (s == common::Range::VALUE_SEARCH_RESULT_EXACT) {
            a_line = range.first.second;
            return true;
        }

        if (a_approximate
            && (s == common::Range::VALUE_SEARCH_RESULT_WITHIN
                || s == common::Range::VALUE_SEARCH_RESULT_BEFORE)) {
            a_line = range.first.second;
            return true;
        }
        return false;
    }

    bool
    line_2_address (Glib::RefPtr<Buffer> a_buf,
                    int a_line,
                    Address &an_address) const
    {
        if (!a_buf)
            return false;

        std::string addr;
        for (Gtk::TextBuffer::iterator it = a_buf->get_iter_at_line (a_line - 1);
             !it.ends_line ();
             ++it) {
            char c = (char) it.get_char ();
            if (isspace (c))
                break;
            addr += c;
        }
        if (!str_utils::string_is_number (addr))
            return false;

        an_address = addr;
        return true;
    }

    bool get_first_asm_address (Address &a_address) const
    {
        if (!asm_ctxt.buffer)
            return false;

        // Get the address of the first line that has an address on it.
        // The assembly buf can contain lines that are not pure asm
        // instruction, e.g. for cases where it contains mixed
        // source/asm instrs.
        int nb_lines = asm_ctxt.buffer->get_line_count ();
        for (int i = 1; i <= nb_lines; ++i)
            if (line_2_address (asm_ctxt.buffer, i, a_address))
                return true;

        return false;
    }

    bool get_last_asm_address (Address &a_address) const
    {
        if (!asm_ctxt.buffer)
            return false;

        // Get the address of the first line -- starting from the end of
        // buf -- that has an address on it.
        // The assembly buf can contain lines that are not pure asm
        // instruction, e.g. for cases where it contains mixed
        // source/asm instrs.
        int nb_lines = asm_ctxt.buffer->get_line_count ();
        for (int i = nb_lines; i >= 1; --i)
            if (line_2_address (asm_ctxt.buffer, i, a_address))
                return true;

        return false;
    }

    //**************
    //<signal slots>
    //**************
    void
    on_marker_region_got_clicked (int a_line, bool a_dialog_requested)
    {
        non_asm_ctxt.marker_region_got_clicked_signal.emit
                                            (a_line, a_dialog_requested);
    }

    void
    on_mark_set_signal (const Gtk::TextBuffer::iterator &a_iter,
                        const Glib::RefPtr<Gtk::TextBuffer::Mark> &a_mark)
    {
        if (a_mark->get_name () == "insert") {
            update_line_col_info_from_iter (a_iter);
        }
    }

    void
    on_signal_insert (const Gtk::TextBuffer::iterator &a_iter,
                      const Glib::ustring &a_text,
                      int a_unknown)
    {
        if (a_text == "" || a_unknown) {}
        update_line_col_info_from_iter (a_iter);
    }

    void
    on_signal_insertion_moved (int a_line, int a_col)
    {
        NEMIVER_TRY

        non_asm_ctxt.current_line = a_line;
        non_asm_ctxt.current_column = a_col;
        update_line_col_label ();

        NEMIVER_CATCH
    }

    void
    on_signal_mark_set (const Gtk::TextBuffer::iterator &a_iter,
                        const Glib::RefPtr<Gtk::TextBuffer::Mark > &a_mark)
    {
        NEMIVER_TRY
        THROW_IF_FAIL (source_view);

        Glib::RefPtr<Gtk::TextBuffer::Mark> insert_mark =
                                source_view->get_buffer ()->get_insert ();
        if (insert_mark == a_mark) {
            insertion_changed_signal.emit (a_iter);
        }
        NEMIVER_CATCH
    }

    //**************
    //</signal slots>
    //**************

    void
    init_signals ()
    {
        source_view->marker_region_got_clicked_signal ().connect
            (sigc::mem_fun (*this,
                            &SourceEditor::Priv::on_marker_region_got_clicked));
        init_assembly_context_signals ();
        init_non_assembly_context_signals ();
    }

    void
    init_common_buffer_signals (Glib::RefPtr<Buffer> a_buf)
    {
        if (!a_buf)
            return;
        a_buf->signal_mark_set ().connect
            (sigc::mem_fun (*this, &SourceEditor::Priv::on_mark_set_signal));
        a_buf->signal_insert ().connect
            (sigc::mem_fun (*this, &SourceEditor::Priv::on_signal_insert));
        a_buf->signal_mark_set ().connect
            (sigc::mem_fun (*this, &SourceEditor::Priv::on_signal_mark_set));
    }

    void
    init_assembly_buffer_signals ()
    {
        Glib::RefPtr<Buffer> buf = asm_ctxt.buffer;
        if (!buf)
            return;
        init_common_buffer_signals (buf);        
    }

    void
    init_assembly_context_signals ()
    {
        init_assembly_buffer_signals ();        
    }

    void init_non_assembly_buffer_signals ()
    {
        Glib::RefPtr<Buffer> buf = non_asm_ctxt.buffer;
        init_common_buffer_signals (buf);
    }

    void
    init_non_assembly_context_signals ()
    {
        non_asm_ctxt.signal_insertion_moved.connect
            (sigc::mem_fun (*this,
                            &SourceEditor::Priv::on_signal_insertion_moved));
        init_non_assembly_buffer_signals ();        
    }

    void
    update_line_col_info_from_iter (const Gtk::TextBuffer::iterator &a_iter)
    {
        SourceEditor::BufferType t = get_buffer_type ();
        if (t == SourceEditor::BUFFER_TYPE_SOURCE) {
            non_asm_ctxt.current_line = a_iter.get_line () + 1;
            non_asm_ctxt.current_column = get_column_from_iter (a_iter);
            non_asm_ctxt.signal_insertion_moved.emit
                                        (non_asm_ctxt.current_line,
                                         non_asm_ctxt.current_column);
        } else if (t == SourceEditor::BUFFER_TYPE_ASSEMBLY) {
            asm_ctxt.current_line = a_iter.get_line () + 1;
            asm_ctxt.current_column = get_column_from_iter (a_iter);
            line_2_address (asm_ctxt.buffer,
                            asm_ctxt.current_line,
                            asm_ctxt.current_address);
        }

    }

    void
    update_line_col_label ()
    {
        UString message;
        message.printf (_("Line: %i, Column: %i"),
                        non_asm_ctxt.current_line,
                        non_asm_ctxt.current_column);
        line_col_label->set_text (message);
    }

    int
    get_column_from_iter (const Gtk::TextBuffer::iterator &a_iter)
    {
        return a_iter.get_line_offset () + 1;
    }

    bool
    get_absolute_resource_path (const UString &a_relative_path,
                                     string &a_absolute_path)
    {
        bool result (false);
        string absolute_path =
            Glib::build_filename (Glib::locale_from_utf8 (root_dir),
                                  a_relative_path);
        if (Glib::file_test (absolute_path,
                             Glib::FILE_TEST_IS_REGULAR
                             | Glib::FILE_TEST_EXISTS)) {
            result = true;
            a_absolute_path = absolute_path;
        } else {
            LOG ("could not find file: " << a_absolute_path);
        }
        return result;
    }

    void
    register_breakpoint_marker_type (const UString &a_name,
                                          const UString &a_image)
    {
        string path;
        if (!get_absolute_resource_path (a_image,
                                         path)) {
            THROW ("could not get path to " + a_image);
        }

        Glib::RefPtr<Gsv::MarkAttributes> attributes = Gsv::MarkAttributes::create ();
        attributes->set_icon (Gio::Icon::create (path));
        source_view->set_mark_attributes (a_name, attributes, 0);
    }

    void
    init ()
    {
        status_box->pack_end (*line_col_label,
                              Gtk::PACK_SHRINK, 6 /* padding */);
        init_signals ();
        source_view->set_editable (false);

        register_breakpoint_marker_type (BREAKPOINT_ENABLED_CATEGORY,
                                         "icons/breakpoint-marker.png");
        register_breakpoint_marker_type
                                (BREAKPOINT_DISABLED_CATEGORY,
                                 "icons/breakpoint-disabled-marker.png");

	register_breakpoint_marker_type (COUNTPOINT_CATEGORY,
					 "icons/countpoint-marker.png");

        // move cursor to the beginning of the file
        Glib::RefPtr<Gtk::TextBuffer> source_buffer = source_view->get_buffer ();
        source_buffer->place_cursor (source_buffer->begin ());
    }

    Priv (Gtk::Window &a_parent_window) :
        parent_window (a_parent_window),
        source_view (Gtk::manage (new SourceView)),
        line_col_label (Gtk::manage (new Gtk::Label)),
        status_box (Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL))),
        non_asm_ctxt (-1, -1)

    {
        init ();
    }

    explicit Priv (Gtk::Window &a_parent_window,
                   const UString &a_root_dir,
                   Glib::RefPtr<Buffer> &a_buf,
                   bool a_assembly) :
        root_dir (a_root_dir),
        parent_window (a_parent_window),
        source_view (Gtk::manage (new SourceView (a_buf))),
        line_col_label (Gtk::manage (new Gtk::Label)),
        status_box (Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL))),
        non_asm_ctxt (-1, -1)
    {
        Glib::RefPtr<Buffer> b;
        b = (a_buf) ? a_buf : source_view->get_source_buffer ();
        if (a_assembly) {
            asm_ctxt.buffer = b;
        } else {
            non_asm_ctxt.buffer = b;
        }
        init ();
    }

    explicit Priv (Gtk::Window &a_parent_window,
                   const UString &a_root_dir,
                   Glib::RefPtr<Buffer> &a_buf) :
        root_dir (a_root_dir),
        parent_window (a_parent_window),
        source_view (Gtk::manage (new SourceView (a_buf))),
        status_box (Gtk::manage (new Gtk::Box (Gtk::ORIENTATION_HORIZONTAL))),
        non_asm_ctxt (-1, -1)
    {
        Glib::RefPtr<Buffer> b;
        b = (a_buf) ? a_buf : source_view->get_source_buffer ();
        non_asm_ctxt.buffer = b;
        init ();
    }
};//end class SourceEditor

void
SourceEditor::init ()
{
    Gtk::ScrolledWindow *scrolled (Gtk::manage (new Gtk::ScrolledWindow));
    scrolled->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolled->add (*m_priv->source_view);
    scrolled->show_all ();
    scrolled->set_shadow_type (Gtk::SHADOW_IN);
    pack_start (*scrolled);
    pack_end (*m_priv->status_box, Gtk::PACK_SHRINK);

    //****************************
    //set line pointer pixbuf
    //****************************
    string path = "";
    if (!m_priv->get_absolute_resource_path ("icons/line-pointer.png", path)) {
        THROW ("could not get path to line-pointer.png");
    }
    Glib::RefPtr<Gsv::MarkAttributes> attributes = Gsv::MarkAttributes::create ();
    attributes->set_icon (Gio::Icon::create (path));
    // show this on top
    source_view ().set_mark_attributes (WHERE_CATEGORY, attributes, 100);
    source_view ().set_show_line_marks (true);
}

/// Constructor of the SourceEditor type.
///
/// \param a_parent_window the parent window of the dialogs used by
/// this editor.
///
/// \param a_root_dir the root directory from where to find the
/// resources of the dialogs used by the editor.
///
/// \param a_buf the buffer containing the source to edit.
///
/// \param a_assembly whether to support asm.
SourceEditor::SourceEditor (Gtk::Window &a_parent_window,
                            const UString &a_root_dir,
                            Glib::RefPtr<Buffer> &a_buf,
                            bool a_assembly) :
    Box (Gtk::ORIENTATION_VERTICAL)
{
    m_priv.reset (new Priv (a_parent_window,
                            a_root_dir,
                            a_buf,
                            a_assembly));
    init ();
}

SourceEditor::~SourceEditor ()
{
    LOG_D ("deleted", "destructor-domain");
}

Gsv::View&
SourceEditor::source_view () const
{
    THROW_IF_FAIL (m_priv && m_priv->source_view);
    return *m_priv->source_view;
}

int
SourceEditor::current_line () const
{
    BufferType type = get_buffer_type ();
    if (type == BUFFER_TYPE_SOURCE)
        return m_priv->non_asm_ctxt.current_line;
    else if (type == BUFFER_TYPE_ASSEMBLY)
        return m_priv->asm_ctxt.current_line;
    else
        return -1;
}

void
SourceEditor::current_line (int a_line)
{
    BufferType type = get_buffer_type ();
    if (type == BUFFER_TYPE_SOURCE)
        m_priv->non_asm_ctxt.current_line = a_line;
    else if (type == BUFFER_TYPE_ASSEMBLY)
        m_priv->asm_ctxt.current_line = a_line;
}

int
SourceEditor::current_column () const
{
    return m_priv->non_asm_ctxt.current_column;
}

void
SourceEditor::current_column (int &a_col)
{
    LOG_DD ("current colnum " << (int) a_col);
    m_priv->non_asm_ctxt.current_column = a_col;
}

/// Return a pointer to the current location selected by the user.
///
///  \return a pointer to an instance of Loc representing the location
///  selected by the user.  If the user hasn't selected any location,
///  then return 0.
const Loc*
SourceEditor::current_location () const
{
    BufferType type = get_buffer_type ();
    switch (type) {
    case BUFFER_TYPE_UNDEFINED:
      break;
    case BUFFER_TYPE_SOURCE: {
        UString path;
        get_path (path);
        THROW_IF_FAIL (!path.empty ());
	if (current_line () >= 0)
	  return new SourceLoc (path, current_line ());
    }
      break;
    case BUFFER_TYPE_ASSEMBLY: {
        Address a;
        if (current_address (a))
	  return new AddressLoc (a);
    }
      break;
    default:
        LOG_ERROR ("Invalid buffer type");
    }

    return 0;
}

bool
SourceEditor::move_where_marker_to_line (int a_line, bool a_do_scroll)
{
    LOG_FUNCTION_SCOPE_NORMAL_DD;

    LOG_DD ("a_line: " << a_line);

    THROW_IF_FAIL (a_line >= 0);

    Gtk::TextIter line_iter =
            source_view ().get_source_buffer ()->get_iter_at_line (a_line - 1);
    if (!line_iter) {
      LOG_DD ("Couldn't find line " << a_line << " in the buffer");
      return false;
    }

    Glib::RefPtr<Gtk::TextMark> where_marker =
        source_view ().get_source_buffer ()->get_mark (WHERE_MARK);
    if (!where_marker) {
        where_marker =
            source_view ().get_source_buffer ()->create_source_mark
                                                        (WHERE_MARK,
                                                         WHERE_CATEGORY,
                                                         line_iter);
        THROW_IF_FAIL (where_marker);
    } else {
        source_view ().get_source_buffer ()->move_mark (where_marker,
                                                        line_iter);
    }
    if (a_do_scroll) {
        scroll_to_line (a_line);
    }
    return true;
}

void
SourceEditor::unset_where_marker ()
{
    Glib::RefPtr<Gtk::TextMark> where_marker =
        source_view ().get_source_buffer ()->get_mark (WHERE_MARK);
    if (where_marker && !where_marker->get_deleted ()) {
        source_view ().get_source_buffer ()->delete_mark (where_marker);
    }
}

bool
SourceEditor::set_visual_breakpoint_at_line (int a_line,
					     bool a_is_count_point,
					     bool enabled)
{
    LOG_FUNCTION_SCOPE_NORMAL_DD


    LOG_DD ("a_line: " << (int)a_line << "enabled: " << (int) enabled);

    // GTK+ Considers that line numbers (in Gtk::TextBuffer) start at 0,
    // where as in Nemiver (mostly due to GDB), we consider that they
    // starts at 1. So adjust a_line accordingly.
    if (a_line <= 0)
        return false;
    --a_line;

    UString marker_type;
    if (enabled) {
      if (a_is_count_point)
	marker_type = COUNTPOINT_CATEGORY;
      else
        marker_type = BREAKPOINT_ENABLED_CATEGORY;
    } else {
        marker_type = BREAKPOINT_DISABLED_CATEGORY;
    }

    std::map<int,
            Glib::RefPtr<Gsv::Mark> > *markers;
    if ((markers = m_priv->get_markers ()) == 0)
        return false;


    Glib::RefPtr<Buffer> buf = source_view ().get_source_buffer ();
    std::map<int, Glib::RefPtr<Gsv::Mark> >::iterator mark_iter;
    mark_iter = markers->find (a_line);
    if (mark_iter !=  markers->end ()) {
        if (!mark_iter->second->get_deleted ()) {
            LOG_DD ("deleting marker");
            buf->delete_mark (mark_iter->second);
            markers->erase (a_line);
        }
    }
    // marker doesn't yet exist, so create one of the correct type
    Gtk::TextIter iter = buf->get_iter_at_line (a_line);
    LOG_DD ("a_line: " << a_line);
    if (!iter) {
      LOG_DD ("Line not found in buffer");
      return false;
    }

    UString marker_name = UString::from_int (a_line);

    LOG_DD ("creating marker of type: " << marker_type);
    Glib::RefPtr<Gsv::Mark> marker;
    marker = buf->create_source_mark (marker_name, marker_type, iter);
    (*markers)[a_line] = marker;
    return true;
}

/// Remove the marker that represent a breakpoint
/// \param a_line the line to remove the breakpoint marker from.
/// Note that a_line starts at 1. Gtk assumes that line numbers start
/// at 0 but in Nemiver we assume -- by convention -- that they start
/// at 1 mostly to comply with GDB.
/// \return true upon successful completion, false otherwise.
bool
SourceEditor::remove_visual_breakpoint_from_line (int a_line)
{
    std::map<int, Glib::RefPtr<Gsv::Mark> > *markers;

    if ((markers = m_priv->get_markers ()) == 0 || a_line < 1)
        return false;

    --a_line;
    std::map<int, Glib::RefPtr<Gsv::Mark> >::iterator iter;
    iter = markers->find (a_line);
    if (iter == markers->end ()) {
        return false;
    }
    if (!iter->second->get_deleted ())
        source_view ().get_source_buffer ()->delete_mark (iter->second);
    markers->erase (iter);
    return true;
}

/// Clear decorations from the source editor.  Decorations are
/// basically the "where-marker" and the breakpoint markers.
void
SourceEditor::clear_decorations ()
{
    std::map<int, Glib::RefPtr<Gsv::Mark> > *markers;
    if ((markers = m_priv->get_markers ()) == 0)
        return;
    typedef std::map<int, Glib::RefPtr<Gsv::Mark> >::iterator
      SourceMarkMapIter;

    std::list<SourceMarkMapIter> marks_to_erase;

    // Clear breakpoint markers and erase them from the hash map that
    // indexes them.
    for (SourceMarkMapIter it = markers->begin ();
	 it != markers->end ();
	 ++it) {
        if (!it->second->get_deleted ()) {
            source_view ().get_source_buffer ()->delete_mark (it->second);
	    // We cannot erase it from its map while walking the map
	    // at the same time.  So let's record that we want to
	    // erase it for now.
	    marks_to_erase.push_front (it);
        }
    }
    // Now erase all the map members that got marked to be erased in
    // the loop above.
    for (std::list<SourceMarkMapIter>::iterator it = marks_to_erase.begin ();
	 it != marks_to_erase.begin ();
	 ++it)
      markers->erase (*it);

    unset_where_marker ();
}

bool
SourceEditor::is_visual_breakpoint_set_at_line (int a_line) const
{
    std::map<int, Glib::RefPtr<Gsv::Mark> > *markers;
    std::map<int, Glib::RefPtr<Gsv::Mark> >::iterator iter;
    if ((markers = m_priv->get_markers ()) == 0)
        return false;
    iter = markers->find (a_line);
    if (iter == markers->end ()) {
        return false;
    }
    return true;
}

struct ScrollToLine {
    int m_line;
    SourceView *m_source_view;

    ScrollToLine () :
        m_line (0),
        m_source_view (0)
    {}

    ScrollToLine (SourceView *a_source_view, int a_line) :
        m_line (a_line),
        m_source_view (a_source_view)
    {
    }

    bool do_scroll ()
    {
        if (!m_source_view) {return false;}
        Gtk::TextIter iter =
            m_source_view->get_buffer ()->get_iter_at_line (m_line);
        if (!iter) {return false;}
        m_source_view->scroll_to (iter, 0.1);
        return false;
    }
};

bool
SourceEditor::scroll_to_line (int a_line)
{
    static ScrollToLine s_scroll_functor;
    s_scroll_functor.m_line = a_line;
    s_scroll_functor.m_source_view = m_priv->source_view;
    Glib::signal_idle ().connect (sigc::mem_fun (s_scroll_functor,
                                                 &ScrollToLine::do_scroll));
    return true;
}

void
SourceEditor::scroll_to_iter (Gtk::TextIter &a_iter)
{
    if (!a_iter) {
        LOG_DD ("iter points at end of buffer");
        return;
    }
    static ScrollToLine s_scroll_functor;
    s_scroll_functor.m_line = a_iter.get_line ();
    s_scroll_functor.m_source_view = m_priv->source_view;
    Glib::signal_idle ().connect (sigc::mem_fun (s_scroll_functor,
                                                 &ScrollToLine::do_scroll));
}

void
SourceEditor::set_path (const UString &a_path)
{
    m_priv->path_ = a_path;
}

void
SourceEditor::get_path (UString &a_path) const
{
    a_path = m_priv->path_;
}

const UString&
SourceEditor::get_path () const
{
    return m_priv->path_;
}

void
SourceEditor::get_file_name (UString &a_file_name)
{
    string path;
    path = Glib::locale_from_utf8 (get_path ());
    path = Glib::path_get_basename (path);
    a_file_name = Glib::locale_to_utf8 (path);
}

///  \return true if a_char represents a variable name delimiter
///  character, false otherwise.
///
///  \param a_char the character to consider.
bool
is_word_delimiter (gunichar a_char)
{
    if (!isalnum (a_char) && a_char != '_') {
        return true;
    }
    return false;
}

/// Parse the name of the variable that surrounds a_iter.  If a_iter
/// is on the "bar" part of "foo->bar", the name returned is
/// "foo->bar".  If it's on the "foo" part, the name returned is
/// "foo".  Similarly for foo.bar.
///
/// \param a_iter the iterator from which the parsing starts
///
/// \param a_begin the resulting iterator pointing at the character
/// that starts the variable name that has been parsed.
///
/// \param a_end the resulting iterator pointing at the character that
/// ends the variable name that has been parsed.
///
/// \return true if the parsing succeeded, false otherwise.
bool
parse_word_around_iter (const Gtk::TextBuffer::iterator &a_iter,
			Gtk::TextBuffer::iterator &a_begin,
			Gtk::TextBuffer::iterator &a_end)
{
    if (!a_iter)
	return false;

    gunichar c = 0, prev_char = 0;
    Gtk::TextBuffer::iterator iter = a_iter;

    // First, go backward to find the first word delimiter before a_iter
    while (iter.backward_char ()
	   && (!is_word_delimiter (c = iter.get_char ())
	       || c == '>' || c == '-' || c == '.')) {
	if (c == '-') {
	    if (prev_char == '>') {
		// this is the '-' of the "->" operator.  Keep going.

	    } else {
		// This is the minus operator.  It's a word
		// delimiter.  Stop here.
		iter.forward_char ();
		break;
	    }
	}
	prev_char = c;
    }
    iter.forward_char ();
    a_begin = iter;

    // Then, go forward find the first word delimiter after a_iter
    iter = a_iter;
    while (iter.forward_char ()
	   && !is_word_delimiter (iter.get_char ())) {}
    a_end = iter;

    return true;
}

bool
SourceEditor::get_word_at_position (int a_x,
				    int a_y,
				    UString &a_word,
				    Gdk::Rectangle &a_start_rect,
				    Gdk::Rectangle &a_end_rect) const
{
    LOG_FUNCTION_SCOPE_NORMAL_DD

    THROW_IF_FAIL (m_priv);
    int buffer_x=0, buffer_y=0;
    source_view ().window_to_buffer_coords (Gtk::TEXT_WINDOW_TEXT,
					    (int)a_x,
					    (int)a_y,
					    buffer_x, buffer_y);
    Gtk::TextBuffer::iterator clicked_at_iter;
    source_view ().get_iter_at_location (clicked_at_iter, buffer_x, buffer_y);
    if (!clicked_at_iter) {
	return false;
    }

    Gtk::TextBuffer::iterator start_word_iter, end_word_iter;
    if (!parse_word_around_iter (clicked_at_iter,
				 start_word_iter,
				 end_word_iter))
	return false;

    UString var_name = start_word_iter.get_slice (end_word_iter);

    Gdk::Rectangle start_rect, end_rect;
    source_view ().get_iter_location (start_word_iter, start_rect);
    source_view ().get_iter_location (end_word_iter, end_rect);
    if (!(start_rect.get_x () <= buffer_x) || !(buffer_x <= end_rect.get_x ())) {
	LOG_DD ("mouse not really on word: '" << var_name << "'");
	return false;
    }
    LOG_DD ("got variable candidate name: '" << var_name << "'");
    a_word = var_name;
    a_start_rect = start_rect;
    a_end_rect = end_rect;
    return true;
}

bool
SourceEditor::do_search (const UString &a_str,
                         Gtk::TextIter &a_start,
                         Gtk::TextIter &a_end,
                         bool a_match_case,
                         bool a_match_entire_word,
                         bool a_search_backwards,
                         bool a_clear_selection)
{
    Glib::RefPtr<Buffer> source_buffer = source_view ().get_source_buffer ();
    THROW_IF_FAIL (source_buffer);

    if (a_clear_selection) {
        source_buffer->select_range (source_buffer->end (),
                                     source_buffer->end ());
    }

    Gtk::TextIter search_iter, limit;
    if (source_view ().get_source_buffer ())
        search_iter =
            source_view ().get_source_buffer ()->get_insert ()->get_iter ();

    if (a_search_backwards) {
        if (!search_iter)
            search_iter = source_buffer->end ();
        search_iter--;
        limit = source_buffer->begin ();
    } else {
        if (!search_iter)
            search_iter = source_buffer->begin ();
        limit = source_buffer->end ();
        limit--;
    }

    Gtk::TextIter start, end;
    if (source_buffer->get_selection_bounds (start, end)) {
        if (a_search_backwards) {
            search_iter = start;
        } else {
            search_iter = end;
        }
    }

    //*********************
    //build search flags
    //**********************
    Gtk::TextSearchFlags search_flags = Gtk::TEXT_SEARCH_TEXT_ONLY;
    if (!a_match_case) {
        search_flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
    }

    bool found=false;
    if (a_search_backwards) {
        if (search_iter.backward_search (a_str, search_flags,
                                         a_start, a_end, limit)) {
            found = true;
        }
    } else {
        if (search_iter.forward_search (a_str, search_flags,
                    a_start, a_end, limit)) {
            found = true;
        }
    }

    if (found && a_match_entire_word) {
        Gtk::TextIter iter = a_start;
        if (iter.backward_char ()) {
            if (!is_word_delimiter (*iter)) {
                found = false;
            }
        }
        if (found) {
            iter = a_end;
            if (!is_word_delimiter (*iter)) {
                found = false;
            }
        }
    }

    if (found) {
        source_buffer->select_range (a_start, a_end);
        scroll_to_iter (a_start);
        return true;
    }
    return false;
}

/// Guess the mime type of a file.
/// \param a_path the path of the file to consider.
/// \param a_mime_type the resulting mime type, set iff the function
/// returns true.
/// \return true upon successful completion, false otherwise.
bool
SourceEditor::get_file_mime_type (const UString &a_path,
                                  UString &a_mime_type)
{
    NEMIVER_TRY

    std::string path = Glib::filename_from_utf8 (a_path);

    Glib::RefPtr<Gio::File> gio_file = Gio::File::create_for_path (path);
    THROW_IF_FAIL (gio_file);

    UString mime_type;
    Glib::RefPtr<Gio::FileInfo> info = gio_file->query_info();
    mime_type = Gio::content_type_get_mime_type(info->get_content_type ());

    if (mime_type == "") {
        mime_type = "text/x-c++";
    }
    LOG_DD ("file has mime type: " << mime_type);
    a_mime_type = mime_type;
    return true;

    NEMIVER_CATCH_AND_RETURN (false)
}

/// Make sure the source buffer a_buf is properly setup to have the
/// mime type a_mime_type. If a_buf is null, a new one is created.
/// Returns true upon successful completion, false otherwise.
bool
SourceEditor::setup_buffer_mime_and_lang (Glib::RefPtr<Buffer> &a_buf,
					  const std::string &a_mime_type)
{
    NEMIVER_TRY

    Glib::RefPtr<LanguageManager> lang_manager =
        LanguageManager::get_default ();
    Glib::RefPtr<Language> lang;
    std::vector<std::string> lang_ids = lang_manager->get_language_ids ();
    for (std::vector<std::string>::const_iterator it = lang_ids.begin ();
         it != lang_ids.end ();
         ++it) {
        Glib::RefPtr<Gsv::Language> candidate =
            lang_manager->get_language (*it);
        std::vector<Glib::ustring> mime_types = candidate->get_mime_types ();
        std::vector<Glib::ustring>::const_iterator mime_it;
        for (mime_it = mime_types.begin ();
             mime_it != mime_types.end ();
             ++mime_it) {
            if (*mime_it == a_mime_type) {
                // one of the mime types associated with this language matches
                // the mime type of our file, so use this language
                lang = candidate;
                break;  // no need to look at further mime types
            }
        }
        // we found a matching language, so stop looking for other languages
        if (lang) break;
    }

    if (!a_buf)
        a_buf = Buffer::create (lang);
    else {
        a_buf->set_language (lang);
        a_buf->erase (a_buf->begin (), a_buf->end ());
    }
    THROW_IF_FAIL (a_buf);
    return true;

    NEMIVER_CATCH_AND_RETURN (false);
}

/// Create a source buffer properly set up to contain/highlight c++
/// code.
/// \return the created buffer or nil if something bad happened.
Glib::RefPtr<Buffer>
SourceEditor::create_source_buffer ()
{
    Glib::RefPtr<Buffer> result;
    setup_buffer_mime_and_lang (result);
    return result;
}

bool
SourceEditor::load_file (Gtk::Window &a_parent,
                         const UString &a_path,
                         const std::list<std::string> &a_supported_encodings,
                         bool a_enable_syntax_highlight,
                         Glib::RefPtr<Buffer> &a_source_buffer)
{
    NEMIVER_TRY;

    std::string path = Glib::filename_from_utf8 (a_path);
    Glib::RefPtr<Gio::File> gio_file = Gio::File::create_for_path (path);
    THROW_IF_FAIL (gio_file);

    if (!gio_file->query_exists ()) {
        LOG_ERROR ("Could not open file " + path);
        ui_utils::display_error (a_parent,
                                 "Could not open file: "
                                 + Glib::filename_to_utf8 (path));
        return false;
    }

    UString mime_type;
    if (!get_file_mime_type (path, mime_type)) {
        LOG_ERROR ("Could not get mime type for " + path);
        return false;
    }

    if (!setup_buffer_mime_and_lang (a_source_buffer, mime_type)) {
        LOG_ERROR ("Could not setup source buffer mime type or language");
        return false;
    }
    THROW_IF_FAIL (a_source_buffer);

    gint buf_size = 10 * 1024;
    CharSafePtr buf (new gchar [buf_size + 1]);
    memset (buf.get (), 0, buf_size + 1);

    unsigned nb_bytes=0;
    std::string content;

    Glib::RefPtr<Gio::FileInputStream> gio_stream = gio_file->read ();
    THROW_IF_FAIL (gio_stream);
    gssize bytes_read = 0;
    for (;;) {
        bytes_read = gio_stream->read (buf.get (), buf_size);
        content.append (buf.get (), bytes_read);
        nb_bytes += bytes_read;
        if (bytes_read != buf_size) {break;}
    }
    gio_stream->close ();

    UString utf8_content;
    std::string cur_charset;
    if (!str_utils::ensure_buffer_is_in_utf8 (content,
                                              a_supported_encodings,
                                              utf8_content)) {
        UString msg;
        msg.printf (_("Could not load file %s because its encoding "
                      "is different from %s"),
                    path.c_str (),
                    cur_charset.c_str ());
        ui_utils::display_error (a_parent, msg);
        return false;
    }
    a_source_buffer->set_text (utf8_content);
    LOG_DD ("file loaded. Read " << (int)nb_bytes << " bytes");

    a_source_buffer->set_highlight_syntax (a_enable_syntax_highlight);

    NEMIVER_CATCH_AND_RETURN (false);

    return true;
}

    /// Given a menu to popup, pop it up at the right place;
    ///
    /// \param a_event the event that triggered this whole button
    /// popup business or NULL.
    ///
    /// \param a_attach_to a widget to attach the popup menu to, or NULL.
    ///
    /// \param a_menu the menu to popup.
void
SourceEditor::setup_and_popup_menu (GdkEventButton *a_event,
                                    Gtk::Widget *a_attach_to,
                                    Gtk::Menu *a_menu)
{
    m_priv->source_view->setup_and_popup_menu (a_event, a_attach_to,
                                               a_menu);
}

/// Add asm instructions to the underlying buffer of a source editor.
///
/// \param a_parent_window the parent window of the dialogs used in
/// the course of adding the asm instruction to the source editor.
///
/// \param a_asm the list of asm instructions to add to the source editor.
///
/// \param a_append whether to append the asm instructions (if true)
/// or to prepend them to the buffer.
///
/// \param a_src_search_dirs if there are source files to find, where
/// to look for them.  This can be useful for mixed (with source code)
/// asm.
///
/// \param a_session_dirs if a file was searched and found, its
/// directory is added to this list.
///
/// \param a_ignore_path, if a source file is searched, is not found,
/// and is in this list, do not ask the user to help us locate it.
///
/// \param a_buf the underlying source buffer of this source editor.
/// It's where the asm instructions are to be added.
///
/// \return true upon successful completion.
bool
SourceEditor::add_asm (Gtk::Window &a_parent_window,
                       const common::DisassembleInfo &/*a_info*/,
                       const std::list<common::Asm> &a_asm,
                       bool a_append,
                       const list<UString> &a_src_search_dirs,
                       list<UString> &a_session_dirs,
                       std::map<UString, bool> &a_ignore_paths,
                       Glib::RefPtr<Buffer> &a_buf)
{
    LOG_FUNCTION_SCOPE_NORMAL_DD;

    if (!a_buf)
        return false;

    nemiver::common::log_asm_insns (a_asm);

    std::list<common::Asm>::const_iterator it = a_asm.begin ();
    if (it == a_asm.end ())
        return true;

    // Write the first asm instruction into a string stream.
    std::ostringstream first_os, endl_os;
    ReadLine reader (a_parent_window,
                     a_src_search_dirs,
                     a_session_dirs,
                     a_ignore_paths,
                     &ui_utils::find_file_and_read_line);
    bool first_written = write_asm_instr (*it, reader, first_os);
    endl_os << std::endl;

    // Figure out where to insert the asm instrs, depending on a_append
    // (either prepend or append it)
    // Also, if a_buf is not empty and we are going to append asm
    // instrs, make sure to add an "end line" before appending our asm
    // instrs.
    Gtk::TextBuffer::iterator insert_it;
    if (a_append) {
        insert_it = a_buf->end ();
        if (a_buf->get_char_count () != 0) {
            insert_it = a_buf->insert (insert_it, endl_os.str ());
        }
    } else {
        insert_it = a_buf->begin ();
    }
    // Really insert the the first asm instrs.
    if (first_written)
        insert_it = a_buf->insert (insert_it, first_os.str ());

    // Append the remaining asm instrs. Make sure to add an "end of line"
    // before each asm instr.
    bool prev_written = true;
    for (++it; it != a_asm.end (); ++it) {
        // If the first item was empty, do not insert "\n" on the first
        // iteration.
        if (first_written && prev_written)
            insert_it = a_buf->insert (insert_it, endl_os.str ());
        first_written = true;
        ostringstream os;
        prev_written = write_asm_instr (*it, reader, os);
        insert_it = a_buf->insert (insert_it, os.str ());
    }
    return true;
}

/// Load the asm instructions referred to by the disassembling
/// information and stuff it into the underlying buffer of a
/// SourceEditor.
///
/// \param a_parent_window the parent window of the dialogs used in
/// the course of adding the asm instruction to the source editor.
///
/// \param a_info some information describing the assembly instruction
/// stream to add to this source editor.
///
/// \param a_asm the asm instruction stream to add to this source
/// editor.
///
/// \param a_append if true, the asm instructions are added to the
/// buffer, otherwise they are prepended to it.
///
/// \param a_src_search_dirs if there are source files to find, where
/// to look for them.  This can be useful for mixed (with source code)
/// asm.
///
/// \param a_session_dirs if a file was searched and found, its
/// directory is added to this list.
///
/// \param a_ignore_path, if a source file is searched, is not found,
/// and is in this list, do not ask the user to help us locate it.
///
/// \param a_buf the underlying source buffer of this source editor.
/// It's where the asm instructions are to be added.
///
/// \return true upon successful completion.
bool
SourceEditor::load_asm (Gtk::Window &a_parent_window,
                        const common::DisassembleInfo &a_info,
                        const std::list<common::Asm> &a_asm,
                        bool a_append,
                        const list<UString> &a_src_search_dirs,
                        list<UString> &a_session_dirs,
                        std::map<UString, bool> &a_ignore_paths,
                        Glib::RefPtr<Buffer> &a_buf)
{
    LOG_FUNCTION_SCOPE_NORMAL_DD;

    NEMIVER_TRY

    std::string mime_type = "text/x-asm";
    if (!setup_buffer_mime_and_lang (a_buf, mime_type)) {
        LOG_ERROR ("Could not setup source buffer mime type of language");
        return false;
    }
    THROW_IF_FAIL (a_buf);

    add_asm (a_parent_window,
             a_info, a_asm, a_append,
             a_src_search_dirs,
             a_session_dirs,
             a_ignore_paths, a_buf);

    NEMIVER_CATCH_AND_RETURN (false)
    return true;
}

/// Registers a assembly source buffer
/// \param a_buf the assembly source buffer
void
SourceEditor::register_assembly_source_buffer
                    (Glib::RefPtr<Buffer> &a_buf)
{
    m_priv->register_assembly_source_buffer (a_buf);
}

SourceEditor::BufferType
SourceEditor::get_buffer_type () const
{
    return m_priv->get_buffer_type ();
}

bool
SourceEditor::current_address (Address &a_address) const
{
  if (get_buffer_type () != BUFFER_TYPE_ASSEMBLY)
    return false;

  a_address = m_priv->asm_ctxt.current_address;
  return true;
}

/// Registers a normal (non-assembly) source buffer.
/// \param a_buf the source buffer to register.
void
SourceEditor::register_non_assembly_source_buffer
                                    (Glib::RefPtr<Buffer> &a_buf)
{
    m_priv->register_non_assembly_source_buffer (a_buf);
}

/// Get the assembly source buffer that was registered, or a NULL
/// pointer if no one was registered before.
/// \return a smart pointer to the source buffer.
Glib::RefPtr<Buffer>
SourceEditor::get_assembly_source_buffer () const
{
    return m_priv->asm_ctxt.buffer;
}

bool
SourceEditor::set_visual_breakpoint_at_address (const Address &a_address,
						bool a_is_count_point,
                                                bool a_enabled)
{
    int line = -1;
    if (!assembly_buf_addr_to_line (a_address, false, line))
        return false;
    return set_visual_breakpoint_at_line (line,
					  a_is_count_point,
					  a_enabled);
}

/// Remove the marker that represents a breakpoint at a given machine
/// address.
/// \param a_address the address from which to remove the breakpoint
/// marker.
/// \return true upon successful completion, false otherwise.
bool
SourceEditor::remove_visual_breakpoint_from_address (const Address &a_address)
{
    int line = -1;
    if (!assembly_buf_addr_to_line (a_address, false, line))
        return false;
    return remove_visual_breakpoint_from_line (line);
}

/// Scroll the instruction which address is specified.
/// \param a_address the instruction which address to scroll to.
/// \param a_approximate if true and if the a_address hasn't been
/// found in the buffer, scroll to the closes address found.
bool
SourceEditor::scroll_to_address (const Address &a_address,
                                 bool a_approximate)
{
    int line = -1;
    if (!assembly_buf_addr_to_line (a_address, a_approximate, line))
        return false;
    return scroll_to_line (line);
}

/// Get the non-assembly source buffer that was registered, or a NULL
/// pointer if no one was registered before.
/// \return a smart pointer to the source buffer.
Glib::RefPtr<Buffer>
SourceEditor::get_non_assembly_source_buffer () const
{
    return m_priv->non_asm_ctxt.buffer;
}

/// Switch the editor to the assembly source buffer that was
/// registered. This function has no effect if no assembly buffer was
/// registered.
/// \return true if the switch was done, false otherwise.
bool
SourceEditor::switch_to_assembly_source_buffer ()
{
    return m_priv->switch_to_assembly_source_buffer ();
}

/// Switch the editor to the non-assembly source buffer that was
/// registered. This function has no effect if no non-assembly source
/// buffer was registered.
/// \return true if the switch was done, false otherwise.
bool
SourceEditor::switch_to_non_assembly_source_buffer ()
{
    RETURN_VAL_IF_FAIL (m_priv && m_priv->source_view, false);

    if (m_priv->asm_ctxt.buffer
        && (m_priv->source_view->get_source_buffer ()
            != m_priv->non_asm_ctxt.buffer)) {
        m_priv->source_view->set_source_buffer (m_priv->non_asm_ctxt.buffer);
        return true;
    }
    return false;
}

/// Give the source line number at which an asm instruction of a given
/// address is.
/// \param a_address the address of the asm instruction to look for.
/// \param a_approximate if true and if the a_addr hasn't been found
/// in the the buffer, then return the closest address
/// found. Otherwise, pretend the we haven't found the address.
bool
SourceEditor::assembly_buf_addr_to_line (const Address &a_addr,
                                         bool a_approximate,
                                         int &a_line) const
{
    Glib::RefPtr<Buffer> buf = get_assembly_source_buffer ();
    return m_priv->address_2_line (buf, a_addr, a_approximate, a_line);
}

bool
SourceEditor::assembly_buf_line_to_addr (int a_line, Address &a_address) const
{
    Glib::RefPtr<Buffer> buf = get_assembly_source_buffer ();
    return m_priv->line_2_address (buf, a_line, a_address);
}

bool
SourceEditor::get_assembly_address_range (Range &a) const
{
    Address addr;
    if (!m_priv->get_first_asm_address (addr))
        return false;
    Range range;
    range.min (addr);
    if (!m_priv->get_last_asm_address (addr))
        return false;
    range.max (addr);
    a = range;
    return true;
}

bool
SourceEditor::move_where_marker_to_address (const Address &a_address,
                                            bool a_do_scroll,
                                            bool a_approximate)
{
    int line = -1;
    if (!assembly_buf_addr_to_line (a_address, a_approximate, line)) {
        return false;
    }
    return move_where_marker_to_line (line, a_do_scroll);
}

bool
SourceEditor::place_cursor_at_line (size_t a_line)
{
    if (a_line == 0)
        return false;
    --a_line;
    Gtk::TextBuffer::iterator iter =
        source_view ().get_buffer ()->get_iter_at_line (a_line);
    if (!iter) {
        return false;
    }
    source_view().get_buffer ()->place_cursor (iter);
    return true;
}

bool
SourceEditor::place_cursor_at_address (const Address &a_address)
{
    if (get_buffer_type () != BUFFER_TYPE_ASSEMBLY)
        return false;
    int line = -1;
    if (!assembly_buf_addr_to_line (a_address, false, line))
        return false;
    return place_cursor_at_line (line);

}

sigc::signal<void, int, bool>&
SourceEditor::marker_region_got_clicked_signal () const
{
    return m_priv->non_asm_ctxt.marker_region_got_clicked_signal;
}

sigc::signal<void, const Gtk::TextBuffer::iterator&>&
SourceEditor::insertion_changed_signal () const
{
    return m_priv->insertion_changed_signal;
}

NEMIVER_END_NAMESPACE (nemiver)
