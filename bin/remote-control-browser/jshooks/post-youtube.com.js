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
	/* Some CSS to enlarge the play button */
	var youtube_css =
		'.ytp-button.ytp-button-play,' +
		'.ytp-button.ytp-button-play:not(.ytp-disabled):focus,' +
		'.ytp-button.ytp-button-play:not(.ytp-disabled):hover {' +
		'	position: absolute;' +
		'	bottom: 50px;' +
		'	left: 50px;' +
		'	background: transparent;' +
		'	background-image: none;' +
		'	width: 0px;' +
		'	height: 0px;' +
		'	border: 100px solid;' +
		'	border-top-color: transparent;' +
		'	border-right-color: transparent;' +
		'	border-left-color: #cc181e;' +
		'	border-bottom-color: transparent;' +
		'}' +
		'.ytp-button.ytp-button-pause,' +
		'.ytp-button.ytp-button-pause:not(.ytp-disabled):focus,' +
		'.ytp-button.ytp-button-pause:not(.ytp-disabled):hover {' +
		'	position: absolute;' +
		'	bottom: 50px;' +
		'	left: 50px;' +
		'	background: transparent;' +
		'	width: 20px;' +
		'	height: 100px;' +
		'	border: 20px solid;' +
		'	border-top: 0px transparent;' +
		'	border-right-color: #cc181e;' +
		'	border-left-color: #cc181e;' +
		'	border-bottom: 0px transparent;' +
		'}';

	/* CSS insertion */
	function insert_css() {
		var css = document.createElement('style');
		css.type = 'text/css';
		css.innerHTML = youtube_css;
		document.getElementsByTagName('head')[0].appendChild(css);
	}

	/* move the play button to the parent container to make overlay work */
	function move_play_button(ev) {
		var el = ev.target;

		/* check if we hit the play button at all */
		if (!el.className || el.className.indexOf('ytp-button-play') < 0)
			return;

		/* check if it somehow has already been moved */
		if (el.parentElement.className.indexOf('html5-player-chrome') < 0)
			return;

		el.parentElement.parentElement.appendChild(el);

		var ytp = document.getElementById('player-api');
		ytp.removeEventListener('DOMNodeInserted', move_play_button, false);
	}

	/* the player is loaded a bit later, making this sentinel necessary */
	function place_button_sentinel() {
		var ytp = document.getElementById('player-api');
		ytp.addEventListener('DOMNodeInserted', move_play_button, false);
	}

	/* Enable html5 video player in favor of flash */
	function force_html5() {
		window.ytspf = window.ytspf || {};
		Object.defineProperty(window.ytspf, 'enabled', { value: false });
		ytplayer.config.html5 = true;
		delete ytplayer.config.args.ad3_module;
	}

	function disable_ajax_navigation() {
		window.spf.dispose();
	}

	function init() {
		insert_css();
		force_html5();
		disable_ajax_navigation();
		place_button_sentinel();
	}

	init();
})();
