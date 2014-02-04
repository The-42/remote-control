/*
 * Copyright (C) 2014 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

'use strict';

(function()
{
	/* Enable message output to console by appending
	 * 	avdglobal_debug_on
	 * as a parameter to your current URL (hence separated with '&'
	 * or '?' if it is the first or only parameter)
	 */

	// wikipedia.org/wiki/YouTube#Quality_and_codecs
	var html5_codecs_available = (function () {
		var q = {};
		var v = document.createElement('video');

		if (v.canPlayType('video/mp4')) {
			q['18'] = 'MP4 360p';
			q['22'] = 'MP4 720p';
			q['37'] = 'MP4 1080p';
			q['38'] = 'MP4 4K';
			q['82'] = 'MP4 3D 360p';
			q['83'] = 'MP4 3D 240p';
			q['84'] = 'MP4 3D 720p';
			q['85'] = 'MP4 3D 520p';
			q['137'] = 'MP4 1080p (silent)';
			q['160'] = 'MP4 144p (silent)';
			q['133'] = 'MP4 240p (silent)';
			q['134'] = 'MP4 360p (silent)';
			q['135'] = 'MP4 480p (silent)';
			q['136'] = 'MP4 720p (silent)';
			q['139'] = 'MP4 Audio 48k';
			q['140'] = 'MP4 Audio 128k';
			q['141'] = 'MP4 Audio 256k';
		}
		if (v.canPlayType('video/webm')) {
			q['43'] = 'WebM 360p';
			q['44'] = 'WebM 480p';
			q['45'] = 'WebM 720p';
			q['46'] = 'WebM 1080p';
			q['100'] = 'WebM 3D 360p (128k)';
			q['101'] = 'WebM 3D 360p (192k)';
			q['102'] = 'WebM 3D 720p';
			q['171'] = 'WebM Audio 128k';
			q['172'] = 'WebM Audio 192k';
		}
		if (v.canPlayType('video/3gpp')) {
			q['13'] = '3GP generic';
			q['17'] = '3GP 144p';
			q['36'] = '3GP 240p';
		}
		return q;
	}());

	var html5_codecs_wanted = (function () {
		var q = [];
		/* only show resolutions below 720p */
		var p = [
		/* MP4 regular */
		'18',
		/* MP4 3D */
		'85', '82', '83',
		/* MP4 silent */
		'135', '134', '133', '160',
		/* 3GP */
		'36', '17', '13'];

		for (var i = 0; i < p.length; i++) {
			if (html5_codecs_available.hasOwnProperty(p[i]))
				q.push(p[i]);
		}
		return q;
	}());

	/* source stream */
	function parseStreamMap()
	{
		var logstring = 'Available streams:';
		var streamMap = {};
		var d;

		if (ytplayer && ytplayer.config && ytplayer.config.args &&
			ytplayer.config.args.hasOwnProperty('url_encoded_fmt_stream_map')) {
			/* YT's own global */
			d = ytplayer.config.args['url_encoded_fmt_stream_map'];
			d = decodeURIComponent(d);
			d = d.replace(/\+codecs=".*?"/g, '');
			d = d.replace(/&type=[^\,&]*/g, '');
			d = d.split(',');
		} else {
			/* streams from player itself */
			d = document.body.innerHTML.match(/"url_encoded_fmt_stream_map":\s"([^"]+)"/);
			if (d) {
				d = d[1].split(',');
			} else {
				/* streams from script updating the player */
				d = document.body.innerHTML.match(/url_encoded_fmt_stream_map=([^;]+)/);
				if (d)
					d = decodeURIComponent(d[1]).split(',');
				else if (document.querySelector('#player-unavailable').className.indexOf('hid') > -1)
					return null;
			}
		}

		if (!d) {
			log.warn('failed to find any streams!');
			return null;
		}

		d.forEach(function (s) {
			var str = decodeURIComponent(s);
			var tag = str.match(/itag=(\d{0,2})/);
			var url = str.match(/url=(.*?)(\\u0026|$)/);
			var sig = str.match(/[sig|s]=([A-Z0-9]*\.[A-Z0-9]*(?:\.[A-Z0-9]*)?)/);

			if (tag && url && sig) {
				tag = tag[1];
				url = url[1];
				sig = sig[1];
			} else {
				return null;
			}

			var has = html5_codecs_available.hasOwnProperty(tag);
			var idx = html5_codecs_wanted.indexOf(tag);
			var tot = html5_codecs_wanted.length;
			logstring += '\n\titag: ' + tag + ' (playable: ' + (has ? 'yes' : 'no') +
				', wanted: ' + (idx > -1 ? ('yes [' + idx + '/' + tot + ']') : 'no') + ')';

			if (html5_codecs_available.hasOwnProperty(tag)) {
				url = url.replace(/[&|\?]itag=\d{0,2}/g, function (match) {
					return match.indexOf('?') !== -1 ? '?' : '';
				});

				streamMap[tag] = decodeURIComponent(url) + '&itag=' + tag + '&signature=' + sig;
			}

		});

		log.info(logstring);

		return streamMap;
	}

	function chooseStream(streams)
	{
		var s_format = null;

		for (var i = 0; i < html5_codecs_wanted.length; i++) {
			if (streams.hasOwnProperty(html5_codecs_wanted[i])) {
				s_format = html5_codecs_wanted[i];
				log.info('Selecting stream ' + s_format +
					' -- ' + html5_codecs_available[s_format]);
				break;
			}
		}

		if (!s_format)
			log.warn('No appropriate stream found!');

		return s_format;
	}

	/* initialization routine */
	function init()
	{
		var yt_video = document.querySelector('#movie_player') ||
			       document.querySelector('#c4-player');
		var own_vid = document.querySelector('#avd-html5-video');

		if (yt_video) {
			log('YT-video found.');

			var yt_player = yt_video.parentNode;
			if (!yt_player) {
				log.error('Failed to find the player or the video.');
				return;
			}
			var yt_html5 = yt_video.querySelector('video');
			if (!yt_html5)
				log.info('The current stream is not a HTML5 format and must be SWF.');

			var yt_content = document.querySelector('#watch7-content');
			var yt_next = document.querySelector('#watch7-playlist-bar-next-button');
			var yt_auto = document.querySelector('#watch7-playlist-bar-autoplay-button');
			log('yt_content: ' + !!yt_content + ', yt_next: ' + !!yt_next + ', yt_auto: ' + !!yt_auto);

			if (yt_html5) {
				yt_html5.addEventListener('timeupdate', function () {
					if (!document.contains(this)) {
						this.pause();
					}
				});
			}

			yt_player.parentNode.replaceChild(own_vid.parentNode, yt_player);
		}

		own_vid.style.display = '';
		own_vid.parentNode.className += 'player-width player-height off-screen-target watch-content player-api';

		var streams = parseStreamMap();
		var best = chooseStream(streams);

		if (best)
			own_vid.src = streams[best];
		else
			log.error('You can\'t play this.');
	}


	/* logging */
	if (!window.console || document.URL.indexOf('avdglobal_debug_on') == -1) {
		window.console = {
			log: function() {},
			info: function() {},
			warn: function() {},
			error: function() {}
		};
	}

	var log = function (stuff) { console.log(stuff); };
	log.info = function (stuff) { console.info('[info] ' + stuff); };
	log.warn = function (stuff) { console.warn('[WARN] ' + stuff); };
	log.error = function (stuff) { console.error('[FAIL] ' + stuff); };

	/* fire! */
	log('-= YT script =-\nCurrent URL: ' + window.location.href);

	if (/^https?:\/\/www\.youtube.com\/watch\?/.test(window.location.href) ||
			/^https?:\/\/www\.youtube.com\/user\/\w+\?/.test(window.location.href)) {
		var content = document.getElementById('content');

		if (content.getElementsByClassName('spf-link').length) {
			/* Original MutationObserver has been replaced with a regular event
			 * listener since the webkit version currently used does not support
			 * MutationObservers yet.
			 */
			document.addEventListener('DOMNodeInserted',
				function (evt) { if (evt.target.id === 'watch7-container') init(); },
				false);
		}

		init();
	}

})();
