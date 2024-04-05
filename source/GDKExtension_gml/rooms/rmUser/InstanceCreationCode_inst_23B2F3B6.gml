
font = fnt_gm_20;
v_align = fa_top;
h_align = fa_left;

text = "(*) will only work on Windows since\nit requires the use of get_integer_async";

// This note is not required on windows
if (os_type == os_windows)
	instance_destroy();
