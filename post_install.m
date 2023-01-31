function post_install(s)
	% Load for doc-cache generation
	wd = pwd();
	cd ("src")
	__xwidgets_load__
	cd (wd)
endfunction