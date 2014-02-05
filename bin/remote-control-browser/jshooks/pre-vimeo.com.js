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
	function empty_container(root)
	{
		var vp = root.getElementsByClassName('player');

		if (!vp.length)
			return;

		vp = vp[0];
		while (vp.childNodes.length > 0) {
			vp.removeChild(vp.childNodes[0]);
		}

		vp.style.cssText = '';
	}

	if (/vimeo.com($|\/$|\/\?|\/\d|\/page:\d)/.test(window.location.href)) {
		document.addEventListener('DOMNodeInserted',
			function (evt) {
				if (evt.target.className === 'player_container')
					empty_container(evt.target);
			},
			false);

		empty_container(document);
	}

})();
