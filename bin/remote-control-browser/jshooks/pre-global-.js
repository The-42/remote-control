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
	/* This file is executed before any website is fully loaded.
	 * Hence functions included here should be either fully website-
	 * independent or at least not rely on a complete DOM.
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
	  * 	avdglobal_list_available_codecs
	  */

	function init()
	{
		if (document.URL.indexOf('avdglobal_list_available_codecs') != -1)
			list_available_codecs();

		var own_vid = _makeVideoPlayer();
		own_vid.style.display = 'None';
		document.body.appendChild(own_vid.parentNode);
		_insert_css();
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

	var html5_player_css =
		'#video-container {' +
		'	position: relative;' +
		'	display: inline-block' +
		'}' +
		'#video-controls {' +
		'	position: absolute;' +
		'	bottom: 0;' +
		'	left: 0;' +
		'	right: 0;' +
		'	padding: 10px;' +
		'	opacity: 0;' +
		'	white-space: nowrap' +
		'	-webkit-transition: opacity .3s;' +
		'	z-index: 42042;' +
		'	background-image: -webkit-gradient(' +
		'		linear,' +
		'		left bottom,' +
		'		left top,' +
		'		color-stop(0.13, rgb(3,113,168)),' +
		'		color-stop(1, rgb(0,136,204))' +
		'	);' +
		'}' +
		'#video-controls:before {' +
		'	vertical-align: middle;' +
		'	display: inline-block;' +
		'	height: 100%;' +
		'	content: "";' +
		'}' +
		'#video-container:hover #video-controls { opacity: .9; }' +
		'button, #video-time {' +
		'	background: rgba(0,0,0,.5);' +
		'	border: 0;' +
		'	color: #EEE;' +
		'	vertical-align: middle;' +
		'	border-radius: 5px;' +
		'	font-family: helvetica, arial;' +
		'	font-weight: bold;' +
		'	font-size: 120%' +
		'	margin: 5px;' +
		'	padding: 5px;' +
		'}' +
		'button:hover { cursor: pointer; }' +
		'#video-container #seek-flexer {' +
		'	display: block;' +
		'	overflow: hidden;' +
		'	padding: 5px;' +
		'}' +
		'#video-controls .lefthand { float: left; }' +
		'#video-controls .righthand { float: right; }' +
		'#video-controls input[type="range"] {' +
		'	-webkit-appearance:none;' +
		'	height: 6px;' +
		'	margin: 10px;' +
		'	background-image:-webkit-gradient(' +
		'		linear,' +
		'		left top,' +
		'		right top,' +
		'		color-stop(0, #add8e6),' +
		'		color-stop(.5, #42abba),' +
		'		color-stop(1, #add8e6)' +
		'	);' +
		'}' +
		'#video-controls input::-webkit-slider-thumb{' +
		'	-webkit-appearance:none;' +
		'	width: 30px;' +
		'	height: 30px;' +
		'	border-radius: 30px;' +
		'	background-image:-webkit-gradient(' +
		'		linear,' +
		'		left top,' +
		'		left bottom,' +
		'		color-stop(0, #fefefe),' +
		'		color-stop(0.49, #dddddd),' +
		'		color-stop(0.51, #d1d1d1),' +
		'		color-stop(1, #a1a1a1)' +
		'	);' +
		'}' +
		'#seek-bar { width: 100%; }' +
		'#volume-bar { width: 20%; }';

	/* HTML5 video element */
	function _makeVideoPlayer()
	{
		function secondsToTimeString(tsecs)
		{
			var s = parseInt(tsecs);
			if (!s)
				return '00:00';

			var day = Math.floor(s / (24 * 3600));
			var hrs = Math.floor(s / 3600) % 24;
			var min = Math.floor(s / 60) % 60;
			var sec = Math.floor(s % 60);
			var tstr = '';
			if (day)
				tstr += day + 'd ';
			if (hrs)
				tstr += (hrs < 10 ? '0' : '') + hrs + ':';
			tstr += (min < 10 ? '0' : '') + min + ':';
			tstr += (sec < 10 ? '0' : '') + sec;

			return tstr;
		}

		var video = document.createElement('video');
		video.id = 'avd-html5-video';
		video.controls = '';
		video.preload = '';
		video.autoplay = '';
		video.textContent = 'No can has HTML5, dude.';

		var playpause = document.createElement('button');
		playpause.id = 'play-pause';
		playpause.className = 'play lefthand';
		playpause.type = 'button';
		playpause.textContent = 'Play';

		var seekbar = document.createElement('input');
		seekbar.id = 'seek-bar';
		seekbar.type = 'range';
		seekbar.value = 0;

		var seekspan = document.createElement('span');
		seekspan.id = 'seek-flexer';
		seekspan.appendChild(seekbar);

		var videotime = document.createElement('span');
		videotime.id = 'video-time';
		videotime.className = 'righthand';
		videotime.appendChild(document.createTextNode('- / -'));

		var mute = document.createElement('button');
		mute.id = 'mute';
		mute.className = 'righthand';
		mute.type = 'button';
		mute.textContent = 'Mute';

		var volumebar = document.createElement('input');
		volumebar.id = 'volume-bar';
		volumebar.className = 'righthand';
		volumebar.type = 'range';
		volumebar.min = 0;
		volumebar.max = 1;
		volumebar.step = 0.1;
		volumebar.value = 1;

		var allspan = document.createElement('span');
		allspan.id = 'video-controls-span';
		/* mind the order, floats first! */
		allspan.appendChild(playpause);
		allspan.appendChild(volumebar);
		allspan.appendChild(mute);
		allspan.appendChild(videotime);
		allspan.appendChild(seekspan);

		var pcontrols = document.createElement('div');
		pcontrols.id = 'video-controls';
		pcontrols.appendChild(allspan);
		pcontrols.allowFullScreen = true;

		var container = document.createElement('div');
		container.id = 'video-container';
		container.appendChild(video);
		container.appendChild(pcontrols);

		/* control callbacks */
		playpause.addEventListener('click', function() {
			var video = document.getElementById('avd-html5-video');

			if (video.paused == true) {
				video.play();
				playpause.textContent = 'Pause';
			} else {
				video.pause();
				playpause.textContent = 'Play';
			}
		});

		mute.addEventListener('click', function() {
			var video = document.getElementById('avd-html5-video');

			if (video.muted == true)
				mute.textContent = 'Mute';
			else
				mute.textContent = 'Unmute';

			video.muted = !video.muted;
		});

		seekbar.addEventListener('change', function() {
			var video = document.getElementById('avd-html5-video');

			video.currentTime = video.duration * (seekbar.value / 100);
		});

		seekbar.addEventListener('mousedown', function() {
			var video = document.getElementById('avd-html5-video');

			video.pause();
		});

		seekbar.addEventListener('mouseup', function() {
			var video = document.getElementById('avd-html5-video');

			video.play();
		});

		video.addEventListener('timeupdate', function() {
			var seekbar = document.getElementById('seek-bar');
			var videotime = document.getElementById('video-time');

			seekbar.value = (100 / video.duration) * video.currentTime;
			videotime.textContent = secondsToTimeString(video.currentTime) +
					' / ' + secondsToTimeString(video.duration);
		});

		volumebar.addEventListener('change', function() {
			var video = document.getElementById('avd-html5-video');

			video.volume = volumebar.value;
		});

		video.addEventListener('click', function (e) {
			var viddiv = document.getElementById('video-controls');

			if (this.paused || this.ended) {
				this.play();
				this.webkitEnterFullscreen();
			} else {
				this.pause();
				this.webkitExitFullscreen();
			}
		});

		video.addEventListener('play', function () {
			if (document.contains(this))
				this.play();
			else
				this.src = '';
		});

		video.addEventListener('emptied', function() {
			var seekbar = document.getElementById('seek-bar');
			var videotime = document.getElementById('video-time');
			var playpause = document.getElementById('play-pause');

			playpause.textContent = 'Play';
			seekbar.value = 0;
			videotime.textContent = secondsToTimeString(0) +
					' / ' + secondsToTimeString(video.duration);
		});

		return video;
	}

	/* CSS insertion */
	function _insert_css() {
		var css = document.createElement('style');
		css.type = 'text/css';
		css.innerHTML = html5_player_css;
		document.getElementsByTagName('head')[0].appendChild(css);
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
