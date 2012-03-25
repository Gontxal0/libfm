namespace Fm {
	// [CCode (cheader_filename = "fm.h")]

	[Compact]
	[CCode (ref_function = "fm_path_ref", unref_function = "fm_path_unref", cname = "FmPath", cprefix = "fm_path_", cheader_filename = "fm-path.h")]
	public class Path {
		public Path.for_str(string str);
		public Path.for_path(string path);
		public Path.for_uri(string uri);
		public Path.for_gfile(GLib.File file);
		public Path.child(Path parent, string name);
		public Path.child_len(Path parent, string name, int len);
		public Path.relative(Path parent, string relative);

		public unowned Path get_parent();
		public unowned string get_basename();

		public static unowned Path get_root(); // /
		public static unowned Path get_home(); // home directory
		public static unowned Path get_desktop(); // $HOME/Desktop
		public static unowned Path get_trash(); // trash:///
		public static unowned Path get_apps_menu(); // menu://applications.menu/

		// FmPathFlags get_flags();
		public bool has_prefix(Path prefix);
		public bool is_native();
		public bool is_trash();
		public bool is_trash_root();
		public bool is_virtual();
		public bool is_local();
		public bool is_xdg_menu();

		public string to_str();
		public string to_uri();
		public GLib.File to_gfile();

		public string display_name(bool human_readable);
		public string display_basename();
		
		public string? get_trash_real_path();

		public uint hash();
		public bool equal(Path* p2);

		public bool equal_str(string str, int n);
		public int depth();
	}

	[Compact]
	[CCode (ref_function = "fm_list_ref", unref_function = "fm_list_unref", cname = "FmList", cprefix = "fm_list_", cheader_filename = "fm-path.h")]
	public class PathList {

		[CCode (cname = "fm_path_list_new", cheader_filename = "fm-path.h")]
		public PathList();

		public void clear();
		public bool is_empty();
		public int get_length();

		public void reverse();
		// public void @foreach(Func<Path> data);
		public unowned GLib.List<Path> find(Path data);
		public unowned GLib.List<Path> find_custom(Path data, GLib.CompareFunc<Path> func);
		public void sort(GLib.CompareDataFunc<Path> func);

		public void push_head(Path data);
		public void push_tail(Path data);
		public void push_nth(Path data, int n);

		public Path pop_head();
		public Path pop_tail();
		public Path pop_nth(int n);

		public unowned Path peek_head();
		public unowned Path peek_tail();
		public unowned Path peek_nth(int n);

		public int index(Path data);

		public void remove(Path data);
		public void remove_all(Path data);

		public void insert_before(GLib.List<Path> sibling, Path data);
		public void insert_after(GLib.List<Path> sibling, Path data);
		public void insert_sorted(GLib.List<Path> sibling, Path data, GLib.CompareDataFunc<Path> func);

		public void push_head_link(GLib.List<Path> l);
		public void push_tail_link(GLib.List<Path> l);
		public void push_nth_link(int n, GLib.List<Path> l);

		public GLib.List<Path> pop_head_link();
		public GLib.List<Path> pop_tail_link();
		public GLib.List<Path> pop_nth_link(int n);

		public unowned GLib.List<Path> peek_head_link();
		public unowned GLib.List<Path> peek_tail_link();
		public unowned GLib.List<Path> peek_nth_link(int n);

		public int link_index(GLib.List<Path> l);
		public void unlink(GLib.List<Path> l);
		public void delete_link(GLib.List<Path> l);
	}


	[Compact]
	[CCode (ref_function = "fm_icon_ref", unref_function = "fm_icon_unref", cname = "FmIcon", cprefix = "fm_icon_", cheader_filename = "fm-icon.h")]
	public class Icon {
		public uint n_ref;
		public GLib.Icon gicon;
		public void* user_data;
	}

	[Compact]
	[CCode (ref_function = "fm_mime_type_ref", unref_function = "fm_mime_type_unref", cname = "FmMimeType", cprefix = "fm_mime_type_", cheader_filename = "fm-mime-type.h")]
	public class MimeType {
		public unowned Fm.Icon get_icon();
		public unowned string get_type();
		public unowned string get_desc();
	}

	[Compact]
	[CCode (ref_function = "fm_file_info_ref", unref_function = "fm_file_info_unref", cname = "FmFileInfo", cprefix = "fm_file_info_", cheader_filename = "fm-file-info.h")]
	public class FileInfo {
		public FileInfo ();
		public FileInfo.from_gfileinfo(Path path, GLib.FileInfo inf);

		public unowned Path get_path();
		public unowned string? get_name();
		public unowned string? get_disp_name();

		public void set_path(Path path);
		public void set_disp_name(string name);

		public int64 get_size();
		public unowned string? get_disp_size();

		public int64 get_blocks();

		public Posix.mode_t get_mode();

		public unowned MimeType get_mime_type();
		public unowned Icon? get_icon();

		public bool is_dir();
		public bool is_symlink();
		public bool is_shortcut();
		public bool is_mountable();
		public bool is_image();
		public bool is_text();
		public bool is_desktop_entry();
		public bool is_unknown_type();
		public bool is_hidden;
		public bool is_executable_type();

		public unowned string? get_target();
		public unowned string? get_collate_key();
		public unowned string? get_desc();
		public unowned string? get_disp_mtime();
		public unowned time_t get_mtime();
		public unowned time_t get_atime();

		public bool can_thumbnail();
	}

	[Compact]
	[CCode (ref_function = "fm_list_ref", unref_function = "fm_list_unref", cname = "FmList", cprefix = "fm_list_", cheader_filename = "fm-file-info.h")]
	public class FileInfoList {

		[CCode (cname = "fm_file_info_list_new", cheader_filename = "fm-file-info.h")]
		public FileInfoList();

		public void clear();
		public bool is_empty();
		public int get_length();

		public void reverse();
		// public void @foreach(Func<FileInfo> data);
		public unowned GLib.List<FileInfo> find(FileInfo data);
		public unowned GLib.List<FileInfo> find_custom(FileInfo data, GLib.CompareFunc<FileInfo> func);
		public void sort(GLib.CompareDataFunc<FileInfo> func);

		public void push_head(FileInfo data);
		public void push_tail(FileInfo data);
		public void push_nth(FileInfo data, int n);

		public FileInfo pop_head();
		public FileInfo pop_tail();
		public FileInfo pop_nth(int n);

		public unowned FileInfo peek_head();
		public unowned FileInfo peek_tail();
		public unowned FileInfo peek_nth(int n);

		public int index(FileInfo data);

		public void remove(FileInfo data);
		public void remove_all(FileInfo data);

		public void insert_before(GLib.List<FileInfo> sibling, FileInfo data);
		public void insert_after(GLib.List<FileInfo> sibling, FileInfo data);
		public void insert_sorted(GLib.List<FileInfo> sibling, FileInfo data, GLib.CompareDataFunc<FileInfo> func);

		public void push_head_link(GLib.List<FileInfo> l);
		public void push_tail_link(GLib.List<FileInfo> l);
		public void push_nth_link(int n, GLib.List<FileInfo> l);

		public GLib.List<FileInfo> pop_head_link();
		public GLib.List<FileInfo> pop_tail_link();
		public GLib.List<FileInfo> pop_nth_link(int n);

		public unowned GLib.List<FileInfo> peek_head_link();
		public unowned GLib.List<FileInfo> peek_tail_link();
		public unowned GLib.List<FileInfo> peek_nth_link(int n);

		public int link_index(GLib.List<FileInfo> l);
		public void unlink(GLib.List<FileInfo> l);
		public void delete_link(GLib.List<FileInfo> l);
	}

	// FmMonitor
	[CCode (cheader_filename = "fm-monitor.h")]
	public GLib.FileMonitor? monitor_directory(GLib.File gf) throws GLib.Error;
	[CCode (cheader_filename = "fm-monitor.h")]
	public GLib.FileMonitor? monitor_lookup_monitor(GLib.File gf);
	[CCode (cheader_filename = "fm-monitor.h")]
	public GLib.FileMonitor? monitor_lookup_dummy_monitor(GLib.File gf);

	[CCode (cname = "FmAppInfo", cprefix = "fm_app_info_", cheader_filename = "fm-app-info.h")]
	namespace AppInfo {
		public bool launch(GLib.List<GLib.File> files, GLib.AppLaunchContext launch_context) throws GLib.Error;
		public bool launch_uris(GLib.List<string> uris, GLib.AppLaunchContext launch_context) throws GLib.Error;
		public bool launch_default_for_uri(string *uri, GLib.AppLaunchContext *launch_context) throws GLib.Error;
		public static unowned GLib.AppInfo create_from_commandline(string commandline, string? application_name, GLib.AppInfoCreateFlags flags) throws GLib.Error;
	}

}
