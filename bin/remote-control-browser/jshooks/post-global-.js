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

	var avdglobal_detect_videos = 0;
	var static_has_videos = 0;
	var swfobjects = [];
	var swfembeds = [];

	function init()
	{
		if (document.URL.indexOf('avdglobal_detect_videos') == -1) {
			avdglobal_detect_videos = 1;
		}

		document.addEventListener('DOMNodeInserted', _trigger_pagechange, false);
		detect_videos(null, null);
		if (swfobjects.length || swfembeds.length)
			block_flash();
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
		var sum;
		var doc;

		if (evt && (!evt.target || !evt.target.tagName))
			return;

		doc = docroot || document;

		/* flash objects */
		objs = doc.getElementsByTagName('object');
		for (var i = 0; i < objs.length; i++) {
			flpath = objs[i].getAttribute('data');
			if ((/\.swf\b/).test(flpath)) {
				flashurls.push(flpath);
				swfobjects.push(objs[i]);
			}
		}

		/* iframes */
		ifrs = doc.getElementsByTagName('iframe');
		for (var i = 0; i < ifrs.length; i++) {
			var isrc = ifrs[i].src || ifrs[i].srcdoc;
			if (isrc && !_strbegins(isrc, 'javascript:') && !_strbegins(document.URL, isrc))
				ifrmurls.push(isrc);

			var idoc = ifrs[i].contentDocument || ifrs[i].contentWindow.document;
			if (idoc)
				detect_videos(null, idoc);
		}

		/* videos */
		vids = doc.getElementsByTagName('video');

		/* embeds */
		embs = doc.getElementsByTagName('embed');
		for (var i = 0; i < embs.length; i++) {
			if (embs[i].hasAttribute('type') && /flash/.test(embs[i].type))
				swfembeds.push(embs[i]);
		}

		sum = flashurls.length + ifrmurls.length + vids.length + embs.length;

		if (!evt && docroot)
			static_has_videos += sum;
		else
			static_has_videos = sum;

		if (!avdglobal_detect_videos)
			return;

		/* Logging part */
		if (evt)
			logstring += '\nInserted element: ' + evt.target.tagName;
		else if (docroot)
			logstring += '\nCall from iframe: ' + _shorten(docroot.URL);
		else
			logstring += '\nPlain call.';

		if (flashurls.length) {
			logstring += '\n' + flashurls.length + ' flash object' +
				(flashurls.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < flashurls.length; i++)
				logstring += '\n\t' + _num(i + 1) + ': ' + _shorten(flashurls[i]);
		} else {
			logstring += '\nno relevant objects found.';
		}

		if (ifrmurls.length) {
			logstring += '\n' + ifrmurls.length + ' iframe' +
				(ifrmurls.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < ifrmurls.length; i++)
				logstring += '\n\t' + _num(i + 1) + ': ' + _shorten(ifrmurls[i]);
		} else {
			logstring += '\nno relevant iframes found.';
		}

		if (!vids.length) {
			logstring += '\nno videos found.';
		} else {
			logstring += '\n' + vids.length + ' video' +
				(vids.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < vids.length; i++)
				logstring += '\n\t' + _num(i + 1) + ': ' + _shorten(vids[i].src);
		}

		if (!embs.length) {
			logstring += '\nno embeds found.';
		} else {
			logstring += '\n' + embs.length + ' embed' +
				(embs.length > 1 ? 's' : '') + ' found:';
			for (var i = 0; i < embs.length; i++)
				logstring += '\n\t' + _num(i + 1) + ': ' + _shorten(embs[i].src);
		}

		logstring += '\n' + sum + ' in this instance, ' +
			static_has_videos + ' total.';

		console.log(logstring);
	}

	function block_flash()
	{
		var el;
		var d;

		if (!(swfobjects.length || swfembeds.length))
			return;

		while (swfobjects.length) {
			el = swfobjects.pop();
			d = _create_flash_subst();
			d.style.height = el.offsetHeight + 'px';
			d.style.width = el.offsetWidth + 'px';
			if (el.parentNode.replaceChild(d, el))
				static_has_videos--;
		}

		while (swfembeds.length) {
			el = swfembeds.pop();
			d = _create_flash_subst();
			d.style.height = el.offsetHeight + 'px';
			d.style.width = el.offsetWidth + 'px';
			if (el.parentNode.replaceChild(d, el))
				static_has_videos--;
		}
	}

	function _create_flash_subst()
	{
		var d = document.createElement('div');
		var p = document.createElement('p');

		p.textContent = 'Sorry mate, flash is not available.';
		p.style.display = 'inline-block';
		p.style.fontSize = '250%';

		d.className = 'avd-flash-subst';
		d.style.cssText = 'background-image:' +
			'-webkit-gradient(' +
			'linear,' +
			'left top,' +
			'right top,' +
			'color-stop(0, #add8e6),' +
			'color-stop(.5, #42abba),' +
			'color-stop(1, #add8e6)' +
			');'
		d.style.textAlign = 'center';
		d.style.verticalAlign = 'middle';
		d.style.display = 'table-cell';
		d.appendChild(p);

		return d;
	}

	function _trigger_pagechange(evt)
	{
		detect_videos(evt, null);
		if (swfobjects.length || swfembeds.length)
			block_flash();
	}

	function _num(x)
	{
		return (x < 10 ? ' ' : '') + x;
	}

	function _shorten(str)
	{
		if (str.length > 120)
			return (str.slice(0, 80) + '...' + str.slice(-37));

		return str;
	}

	function _strbegins(str, substr)
	{
		return str.slice(0, substr.length) == substr;
	}

	if (!window.console)
		window.console = { log: function() {} };

	init();
})();
