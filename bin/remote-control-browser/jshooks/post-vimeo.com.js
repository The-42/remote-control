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

	/* Vimeo available codecs and resolutions, in checking order */
	var wantedcodecs = ['h264', 'vp6'];
	var formats = ['hd', 'sd', 'mobile'];
	/* play anything below 720p */
	var RESLIMIT = 720;

	/* data link detection */
	function getDataLink()
	{
		/* Vimeo apparently removes the link to their video stream
		 * right upon page load, so the player has the URL but
		 * nobody else. Regardless, the browser has to know it,
		 * thus it is transferred - request a retransfer of the
		 * raw content and extract the link from there >:>
		 */
		var xhrbase = new XMLHttpRequest();
		/* note that this is synchronous and may take a moment */
		xhrbase.open('GET', window.location.href, false);
		xhrbase.send();
		if (xhrbase.status != 200) {
			log.error('XMLHttpRequest (base) failed with HTTP ' +
				xhrbase.status + ' / ' + xhrbase.statusText);
			return null;
		}

		var cfgurl = xhrbase.responseText.match(/data-config-url="(.*?)"/);
		if (!cfgurl) {
			log.error('Vimeo data config URL not detected');
			return null;
		}
		cfgurl = cfgurl[1].replace(/&amp;/g, '&');

		return cfgurl;
	}

	/* stream link detection */
	function getSrcLink(datalink)
	{
		var xhrjson = new XMLHttpRequest();
		xhrjson.open('GET', datalink, false);
		xhrjson.send();
		if (xhrjson.status != 200) {
			log.error('XMLHttpRequest (data) failed with HTTP ' +
				xhrbase.status + ' / ' + xhrbase.statusText);
			return null;
		}

		var jsondata = JSON.parse(xhrjson.responseText);
		if (!(jsondata.hasOwnProperty('request') &&
				jsondata['request'].hasOwnProperty('files'))) {
			log.error('Incomplete or damaged JSON object retrieved');
			return null;
		}

		var jsoncodecs = jsondata['request']['files'];
		var jsonpcdc = null;
		var h = RESLIMIT;
		var w = RESLIMIT;
		var url = null;

		for (var i = 0; i < wantedcodecs.length; i++) {
			if (!jsoncodecs.hasOwnProperty(wantedcodecs[i]))
				continue;
			jsonpcdc = jsoncodecs[wantedcodecs[i]];
			for (var j = 0; j < formats.length; j++) {
				if (!jsonpcdc.hasOwnProperty(formats[j]))
					continue;
				h = jsonpcdc[formats[j]]['height'];
				w = jsonpcdc[formats[j]]['width'];
				if (w < RESLIMIT && h < RESLIMIT) {
					url = jsonpcdc[formats[j]]['url'];
					log.info('Selecting ' + wantedcodecs[i] +
						' codec, ' + formats[j] +
						' resolution (' + w + 'x' + h + ')');
					break;
				}
			}

		}

		if (url)
			return { url: url, height: h, width: w };

		log.error('No suitable stream available ' +
			JSON.stringify(jsoncodecs.keys, null, 4));
		return null;
	}

	/* related video link detection */
	function checkLinks(tg)
	{
		var gal = document.getElementById('featured_videos');
		var lilist = [];
		var li;
		var a;

		if (!gal)
			return;

		if (tg && tg.parentNode.id == 'featured_videos') {
			lilist.push(tg);
		} else {
			for (var i = 0; i < gal.childNodes.length; i++) {
				li = gal.childNodes[i];
				if (li.tagName == 'LI')
					lilist.push(li);
			}
		}

		for (var i = 0; i < lilist.length; i++) {
			li = lilist[i];
			for (var j = 0; j < li.childNodes.length; j++) {
				a = li.childNodes[j];
				if (a.tagName == 'A')
					a.addEventListener('click', rescueVideo, false);
			}
		}
	}

	function rescueVideo()
	{
		var own_vid = document.querySelector('#avd-html5-video');

		if (own_vid)
			document.body.appendChild(own_vid.parentNode);
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
	log('-= Vimeo script =-\n\tCurrent URL: ' + window.location.href);

	function init(dataurl)
	{
		var vimplayer = document.getElementsByClassName('player')[0];
		var own_vid = document.querySelector('#avd-html5-video');
		var datalink = dataurl ? dataurl : getDataLink();
		var stream;

		if (!own_vid) {
			log.error('player not found!');
			return;
		}

		if (!datalink)
			return;

		if (vimplayer)
			vimplayer.parentNode.replaceChild(own_vid.parentNode, vimplayer);

		stream = getSrcLink(datalink);

		if (own_vid && stream) {
			own_vid.parentNode.style.display = '';
			own_vid.src = stream.url;
			own_vid.style.height = stream.height + 'px';
			own_vid.style.width = stream.width + 'px';
			own_vid.resize();
			log('Playing link: ' + stream.url);
		}
	}

	if (/vimeo.com($|\/$|\/\?|\/\d|\/page:\d)/.test(window.location.href)) {
		document.addEventListener('DOMNodeInserted',
			function (evt) {
				if (evt.target.className === 'player_container') {
					var vp = evt.target.getElementsByClassName('player')[0];
					var datalink = null;
					if ('data-config-url' in vp.attributes)
						datalink = vp.getAttribute('data-config-url');
					init(datalink);
				} else if (evt.target.tagName == 'LI') {
					checkLinks(evt.target);
				}
			},
			false);

		checkLinks();
		init(null);
	}

})();
