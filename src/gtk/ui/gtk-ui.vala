namespace Fm {

public class GtkUI : Object, UI {
	public GtkUI(Gtk.Window? parent_window = null) {
	}

	public void show_error(string message, string? title = null, bool use_markup = true) {
		stderr.printf("title: %s\nmessage: %s\n", title, message);
	}

	public bool ask_ok_cancel(string question, string? title = null, bool use_markup = true) {
		stderr.printf("title: %s\nquestion: %s\n", title, question);
		return false;
	}

	public bool ask_yes_no(string question, string? title = null, bool use_markup = true) {
		stderr.printf("title: %s\nquestion: %s\n", title, question);
		return false;
	}

	public int ask(string question, string? title, bool use_markup, ...) {
		
		return 0;
	}

	public void open_folder(Fm.Path folder) {
		// FIXME: how to do this correctly?
	}

}

public class GtkFileJobUI : GtkUI, FileJobUI {

	public GtkFileJobUI(Gtk.Window? parent_window = null) {
		parent_win = parent_window;
		default_rename_result = RenameResult.NONE;
	}

	~GtkFileJobUI() {
		if(show_dlg_timeout_id != 0)
			Source.remove(show_dlg_timeout_id);
	}

	// initialize the ui with the job
	public void init_with_job(FileJob job, string title, string action, bool has_dest) {
		this.title = title;
		this.action_label_text = action;
		this.has_dest = has_dest;
		this.job = job;
	}

	// called by FileJob to inform the start of the job
	public void start() {
		show_dlg_timeout_id = Timeout.add(1000, on_show_dlg_timeout);
	}

	// called by FileJob to inform the end of the job
	// though FileJob emits "finished" signal, it's easier to just call a function
	public void finish() {
		if(dlg != null) {
			dlg.destroy();
		}
		else if(show_dlg_timeout_id != 0) {
			Source.remove(show_dlg_timeout_id);
			show_dlg_timeout_id = 0;
		}
	}

	// called by FileJob to show total amount of the job, may be called for many times
	public void set_total_amount(uint64 size, int n_files, int n_dirs) {
		// total_size = size;
		// n_total_files = n_files;
		// n_total_dirs = n_dirs;
	}

	// get prepared and ready to do the file operation
	public void set_ready() {
	}

	// progress info, in %
	public void set_percent(int percent) {
		if(progress_bar != null) {
			progress_bar.set_fraction((double)percent/100);
			progress_bar.set_text("%d %%".printf(percent));
		}
		this.percent = percent;
	}

	// estimated time
	public void set_times(uint elapsed, uint remaining) {
		if(remaining_time_label != null) {
			char time_str[32];
			uint secs = remaining;
			uint mins = 0;
			uint hrs = 0;
			if(secs > 60)
			{
				mins = secs / 60;
				secs %= 60;
				if(mins > 60)
				{
					hrs = mins / 60;
					mins %= 60;
				}
			}
			remaining_time_label.set_text("%02u:%02u:%02u".printf(hrs, mins, secs));
		}
	}

	// set src/dest paths currently being handled
	public void set_current_src_dest(Path src_path, Path? dest_path = null) {
		// stdout.printf("%s\n", src_path.display_name(true));
		current_src_path = src_path;
		current_dest_path = dest_path;
		if(src_path_label != null && src_path != null)
			src_path_label.set_label(src_path.display_name(true));
		if(dest_path_label != null && dest_path != null)
			dest_path_label.set_label(dest_path.display_name(true));
	}

	// set currently processed child file/folder and optionally, its dest
	public void set_currently_processed(File src, GLib.FileInfo? src_info, File? dest = null) {
		// consider showing basename here
		current_src_file = src;
		current_src_info = src_info;
		current_dest_file = dest;
		if(current_file_label != null && src_info != null) {
			current_file_label.set_label(src_info.get_display_name());
		}
	}

	// ask for a new file name
	public RenameResult ask_rename(File src_file, GLib.FileInfo src_info, File dest_file, GLib.FileInfo dest_info, out File new_dest) {
		new_dest = null;
		// if we have default action, don't bother the user with further dialogs
		if(default_rename_result == RenameResult.OVERWRITE ||
			default_rename_result == RenameResult.SKIP)
			return default_rename_result;

		show_dialog(); // show the progress dialog and use it as the parent window

		var rename_dlg = new RenameDialog(dlg);
		if(src_info != null) { // display info of the source file
			var src_path = new Path.for_gfile(src_file);
			var fi = new Fm.FileInfo.from_gfileinfo(src_path, src_info);
			rename_dlg.set_src_info(fi);
		}
		if(dest_info != null) { // display info of the destination file which already exists
			var dest_path = new Path.for_gfile(dest_file);
			var fi = new Fm.FileInfo.from_gfileinfo(dest_path, dest_info);
			rename_dlg.set_dest_info(fi);
		}

		// show a dialog for the user to enter a new filename
		var result = rename_dlg.run();
		if(result == RenameResult.RENAME) {
			var new_name = rename_dlg.get_new_name();
			if(new_name != null)
				new_dest = dest_file.get_parent().get_child(new_name);
		}
		// if the user choose to apply the option to all existing files, set it default
		if(rename_dlg.get_apply_to_all())
			default_rename_result = result;

		return result;
	}

	public ErrorAction handle_error(Error err, Severity severity) {
		stdout.printf("error: %s\n", err.message);
		return ErrorAction.CONTINUE;
	}

	private void on_dlg_response(Gtk.Dialog dlg, int response) {
		if(response == Gtk.ResponseType.CANCEL || response == Gtk.ResponseType.DELETE_EVENT)
		{
			if(job != null) {
				job.cancel();
			}
			dlg.destroy();
		}
		else if(response == Gtk.ResponseType.CLOSE) {
			dlg.destroy();
		}
	}

	private bool on_show_dlg_timeout() {
		show_dlg_timeout_id = 0;
		show_dialog();
		return false;
	}

	protected void show_dialog() {
		if(dlg != null) {
			dlg.show();
			return;
		}

		var builder = new Gtk.Builder();
		const string ui_file = Config.PACKAGE_UI_DIR + "/progress.ui";
		builder.set_translation_domain(Config.GETTEXT_PACKAGE);
		builder.add_from_file(ui_file);
		dlg = (Gtk.Dialog)builder.get_object("dlg");
		dlg.response.connect(on_dlg_response);

		var dest_label = (Gtk.Label)builder.get_object("to_label");
		dest_path_label = (Gtk.Label)builder.get_object("dest");
		var icon_widget = (Gtk.Image)builder.get_object("icon");
		msg_label = (Gtk.Label)builder.get_object("msg");
		var action_label = (Gtk.Label)builder.get_object("action");
		src_path_label = (Gtk.Label)builder.get_object("src");
		current_file_label = (Gtk.Label)builder.get_object("current");
		progress_bar = (Gtk.ProgressBar)builder.get_object("progress");
		error_pane = (Gtk.VBox)builder.get_object("error_pane");
		error_msg_text_view = (Gtk.TextView)builder.get_object("error_msg");
		remaining_time_label = (Gtk.Label)builder.get_object("remaining_time");

		var tag_table = new Gtk.TextTagTable();
		bold_tag = new Gtk.TextTag("bold");
		bold_tag.weight = Pango.Weight.BOLD;
		tag_table.add(bold_tag);
		error_buf = new Gtk.TextBuffer(tag_table);
		error_msg_text_view.set_buffer(error_buf);

		// set the src & dest label
		dlg.set_title(title);
		action_label.set_markup(@"<b>$action_label_text</b>");
		if(!has_dest) {
			dest_label.destroy();
			dest_path_label.destroy();
			dest_path_label = null;
		}
		if(current_src_path != null)
			set_current_src_dest(current_src_path, current_dest_path);
		if(current_src_file != null && current_src_info != null)
			set_currently_processed(current_src_file, current_src_info, current_dest_file);
		if(percent > 0.0)
			set_percent(percent);

		dlg.present();
		show_dlg_timeout_id = 0;
	}

	private string title;
	private string action_label_text;
	private bool has_dest;
	private int percent;
	private Path current_src_path;
	private Path? current_dest_path;
	private File current_src_file;
	private GLib.FileInfo? current_src_info;
	private File? current_dest_file;

	private uint64 total_size;
	private int total_n_files;
	private int total_n_dirs;
	private time_t time_remaining;
	
	private RenameResult default_rename_result;

	private Gtk.Window? parent_win;
	private unowned Gtk.Dialog? dlg;
	private unowned Gtk.Label dest_path_label;
	private unowned Gtk.Label msg_label;
	private unowned Gtk.Label src_path_label;
	private unowned Gtk.Label current_file_label;
	private unowned Gtk.ProgressBar progress_bar;
	private unowned Gtk.VBox error_pane;
	private unowned Gtk.TextView error_msg_text_view;
	private unowned Gtk.Label remaining_time_label;
	private Gtk.TextTag bold_tag;
	private Gtk.TextBuffer error_buf;

	private uint show_dlg_timeout_id;

	private unowned FileJob? job;
}

}
