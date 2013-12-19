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
	/* Not all functions in this file are enabled by default since
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
	  * 	avdglobal_list_available_codecs
	  */

	function init()
	{
		if (document.URL.indexOf('avdglobal_list_available_codecs') != -1)
			list_available_codecs();
	}

	function list_available_codecs()
	{
		var codecs = _align_widths([
			'application/ogg',
			'application/ogg; codecs=bogus',
			'application/mp4',
			'application/mp4; codecs=bogus',
			'application/octet-stream',
			'application/octet-stream; codecs=bogus',
			'audio/3gpp',
			'audio/3gpp2',
			'audio/aac',
			'audio/x-aac',
			'audio/aiff',
			'audio/x-aiff',
			'audio/ac3',
			'audio/x-ac3',
			'audio/basic',
			'audio/flac',
			'audio/x-flac',
			'audio/mid',
			'audio/midi',
			'audio/x-midi',
			'audio/mpeg',
			'audio/x-mpeg',
			'audio/mpegurl',
			'audio/x-mpegurl',
			'audio/mp4',
			'audio/mp4; codecs=bogus',
			'audio/ogg',
			'audio/ogg; codecs=bogus',
			'audio/wav',
			'audio/wav; codecs=0',
			'audio/wav; codecs=1',
			'audio/wav; codecs=2',
			'audio/wave',
			'audio/wave; codecs=0',
			'audio/wave; codecs=1',
			'audio/wave; codecs=2',
			'audio/x-wav',
			'audio/x-wav; codecs=0',
			'audio/x-wav; codecs=1',
			'audio/x-wav; codecs=2',
			'audio/x-pn-wav',
			'audio/x-pn-wav; codecs=0',
			'audio/x-pn-wav; codecs=1',
			'audio/x-pn-wav; codecs=2',
			'video/3gpp',
			'video/3gpp2',
			'video/avi',
			'video/mpeg',
			'video/x-mpeg',
			'video/mp4',
			'video/mp4; codecs=bogus',
			'video/msvideo',
			'video/x-msvideo',
			'video/quicktime',
			'video/ogg',
			'video/ogg; codecs=bogus',
			'video/mp4; codecs="avc1.42E01E, mp4a.40.2"',
			'video/mp4; codecs="avc1.58A01E, mp4a.40.2"',
			'video/mp4; codecs="avc1.4D401E, mp4a.40.2"',
			'video/mp4; codecs="avc1.64001E, mp4a.40.2"',
			'video/mp4; codecs="mp4v.20.8, mp4a.40.2"',
			'video/mp4; codecs="mp4v.20.240, mp4a.40.2"',
			'video/3gpp; codecs="mp4v.20.8, samr"',
			'video/ogg; codecs="theora, vorbis"',
			'video/ogg; codecs="theora, speex"',
			'audio/ogg; codecs=vorbis',
			'audio/ogg; codecs=speex',
			'audio/ogg; codecs=flac',
			'video/ogg; codecs="dirac, vorbis"',
			'video/x-matroska; codecs="theora, vorbis"',
			'audio/webm',
			'audio/webm; codecs=vorbis',
			'video/webm',
			'video/webm; codecs=vorbis',
			'video/webm; codecs=vp8',
			'video/webm; codecs=vp8.0',
			'video/webm; codecs="vp8, vorbis"',
		]);

		var vid = document.createElement('video');
		var statement;
		var str = '\n';

		str += '======================================================================\n';
		str += 'Codec support listing for this browser (probably / maybe / no):\n';
		for (var i = 0; i < codecs.length; i++) {
			statement = vid.canPlayType(codecs[i]);
			if (!statement)
				statement = 'no';
			str += '\t' + codecs[i] + ' -- ' + statement + '\n';
		}
		str += '======================================================================';
		console.log(str);
	}

	function _align_widths(arr)
	{
		var max_length = 0;

		for (var i = 0; i < arr.length; i++) {
			if (arr[i].length > max_length)
				max_length = arr[i].length;
		}

		for (var i = 0; i < arr.length; i++) {
			while (arr[i].length < max_length)
				arr[i] += ' ';
		}

		return arr;
	}

	init();
})();
