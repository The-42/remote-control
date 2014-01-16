/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

'use strict';

(function ()
{
	/* This file is executed after the DOM of any website is fully
	 * loaded. Functions included here may rely on a completed DOM.
	 * Rather generic testing or production functions not requiring
	 * a complete DOM should be considered residing in the global
	 * hook file pre-global-.js which is executed before this hook.
	 *
	 * Not all functions in this file are enabled by default since
	 * some only matter for testing or checking once per browser
	 * build. All functions listed can be activated by appending a
	 * parameter tag in the url. The tag consists of the namespace
	 * 'avd_global', an optional negation 'no_' and the full
	 * function name. An actual value for the parameter is not
	 * necessary, since only its mere existence will be checked.
	 *
	 * Append a parameter using the question mark '?', concatenate
	 * multiple parameter with the ampersand '&'.
	 * Beware: If a site already uses url parameter, do not use
	 * another question mark - this will break the site. Instead,
	 * append with the ampersand. Due to the chosen namespace, this
	 * should not have any side effects.
	 *
	 * To call a function test_stuff() on the domain www.example.com
	 * one would type:
	 * 	www.example.com/?avdglobal_test_stuff
	 *
	 * The following url already uses parameter:
	 * 	www.example.com/view.html?start=10
	 * In this case, append with an ampersand, as you would do with
	 * multiple parameter:
	 * 	www.example.com/view.html?start=10&avdglobal_test_stuff
	 *
	 * To deny the always active function detect_this() on the first
	 * example, one would use:
	 * 	www.example.com/?avdglobal_no_detect_this
	 */

	 /* Available parameters:
	  * 	avdglobal_detect_videos
	  */

	function init()
	{
		if (document.URL.indexOf('avdglobal_detect_videos') != -1) {
			document.addEventListener('DOMNodeInserted', detect_videos, false);
			detect_videos(null, null);
		}
	}

	function detect_videos(evt, docroot)
	{
		var logstring = '';
		var flashurls = [];
		var ifrmurls = [];
		var flpath;
		var objs;
		var vids;
		var ifrs;
		var embs;
		var doc;

		if (evt && (!evt.target || !evt.target.tagName))
			return;

		if (evt)
			logstring += '\nInserted element: ' + evt.target.tagName;
		else if (docroot)
			logstring += '\nCall from iframe: ' + docroot.URL;
		else
			logstring += '\nPlain call.';

		doc = docroot || document;

		/* flash objects */
		objs = doc.getElementsByTagName('object');
		for (var i = 0; i < objs.length; i++) {
			flpath = objs[i].getAttribute('data');
			if ((/\.swf\b/).test(flpath))
				flashurls.push(flpath);
		}

		if (flashurls.length) {
			logstring += '\n' + flashurls.length + ' flash object' +
				(flashurls.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < flashurls.length; i++)
				logstring += '\n\t' + (i + 1) + ': ' + flashurls[i];
		} else {
			logstring += '\nno relevant objects found.';
		}

		/* iframes */
		ifrs = doc.getElementsByTagName('iframe');
		for (var i = 0; i < ifrs.length; i++) {
			if (ifrs[i].src && !_strbegins(ifrs[i].src, 'javascript:'))
				ifrmurls.push(ifrs[i].src);
			else if (ifrs[i].srcdoc && !_strbegins(ifrs[i].srcdoc, 'javascript:'))
				ifrmurls.push(ifrs[i].srcdoc);

			var idoc = ifrs[i].contentDocument || ifrs[i].contentWindow.document;
			if (idoc)
				detect_videos(null, idoc);
		}

		if (ifrmurls.length) {
			logstring += '\n' + ifrmurls.length + ' iframe' +
				(ifrmurls.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < ifrmurls.length; i++)
				logstring += '\n\t' + (i + 1) + ': ' + ifrmurls[i];
		} else {
			logstring += '\nno relevant iframes found.';
		}

		/* videos */
		vids = doc.getElementsByTagName('video');
		if (!vids.length) {
			logstring += '\nno videos found.';
		} else {
			logstring += '\n' + vids.length + ' video' +
				(vids.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < vids.length; i++)
				logstring += '\n\t' + (i + 1) + ': ' + vids[i].src;
		}

		/* embeds */
		embs = doc.getElementsByTagName('embed');
		if (!embs.length) {
			logstring += '\nno embeds found.';
		} else {
			logstring += '\n' + embs.length + ' embed' +
				(embs.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < embs.length; i++)
				logstring += '\n\t' + (i + 1) + ': ' + embs[i].src;
		}

		console.log(logstring);
	}

	function _strbegins(str, substr)
	{
		return str.slice(0, substr.length) == substr;
	}

	if (!window.console)
		window.console = { log: function() {} };

	init();
})();
