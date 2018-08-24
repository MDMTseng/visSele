(function($) {
	$.fn.jsonPainter = function(options) {

		// Merge with default options
		var mSettings = $.extend({
			data: null,
			animate: true,
			onParseError: defaultOnParseError
		}, { data : options });

		// For each data
		return this.each(function() {
			console.log('#');
			$this = $(this);

			// Get the json
			try {
				var jsonData = mSettings.data != null ? mSettings.data : $this.text();
				if(typeof jsonData == 'string') {
					if(jsonData == '') return $this.text('');

					// Make sure we start properly
					// jsonData = jsonData.replace(/^.*?{/, '{').replace(/}.*?$/, '}');
					jsonData = jsonData.replace(/^[\s]+|[\s]+$/g, '');
					if(jsonData.charAt(0) != '{' || jsonData.charAt(jsonData.length-1) != '}') {
						throw "JSON not valid";
					}

					eval('jsonData = ' + jsonData + ';');
				}
			} catch(e) {
				return mSettings.onParseError(jsonData, $this, e);
			}

			// Append styles
			var style =
				"<style>" +
					".jsonpainter { font: normal 1.1em monospace; }" +

					".jsonpainter ul, .jsonpainter ol { list-style: none outside none; margin: 0 0 0 2em; padding: 0; }" +
					".jsonpainter li { position: relative; }" +

					".jsonpainter-array { list-style: none; }" +
					".jsonpainter-object { list-style: none; }" +

					".jsonpainter-key { font-style: normal; font-weight: bold; color: #111; }" +
					".jsonpainter-key-collapsable { -moz-user-select: none; cursor: pointer; font-weight: normal; }" +
					".jsonpainter-key i { left: -1em; position: absolute; }" +
					".jsonpainter-key span { font-weight: bold; }" +

					".jsonpainter u { outline: 1px solid #ddd; background: #eee; text-decoration: none; }" +

					".jsonpainter-key { }" +
					".jsonpainter-value { }" +
					".jsonpainter-value-url { }" +
					".jsonpainter-value-null { color: gray; font-style: italic; }" +
					".jsonpainter-value-number { color: blue; }" +
					".jsonpainter-value-string { color: green; }" +
					".jsonpainter-value-string:after { content: '\"'; }" +
					".jsonpainter-value-string:before { content: '\"'; }" +
					".jsonpainter-value-boolean { font-weight: bold; }" +
					".jsonpainter-value-boolean-true { color: darkcyan; }" +
					".jsonpainter-value-boolean-false { color: darkred; }" +
				"</style>";
			$('head').prepend(style);

			// Build the DOM
			$this.text('{');
			$this.addClass('jsonpainter');
			var parent = $('<ul class="jsonpainter-object">');
			parent.appendTo($this);
			buildNode(jsonData, parent);
			$this.append('}');
		});

		// Build DOM structure from JSON
		function buildNode(node, parent) {
			parent.text('');

			// Get the count
			var length = 0;
			for(var key in node) if(node.hasOwnProperty(key)) {
				length++;
			}

			// Iterate over every node
			var i = 0;
			for(var key in node) if(node.hasOwnProperty(key)) {
				var value = node[key];

				// Clear keys for arrays
				if(node instanceof Array) {
					key = '';
				}

				// Placeholder for this item
				var item = $('<li class="jsonpainter-item">').appendTo(parent);

				// Create key
				_key = $('<em class="jsonpainter-key">').text(node instanceof Array ? '' : key + ': ');
				item.append(_key);

				// Recursive
				if(value != null && typeof value == 'object') {
					var map = value instanceof Array ? [ 'ol', '[' , ']' ] : [ 'ul', '{' , '}' ];

					// Collapse
					if(value != null && typeof value == 'object') {

						// Expand key with parentesis
						if(key != '') key= ' <span>' + key + '</span>: ';
						_key.html('<i>-&nbsp;</i>' + key + map[1]);

						// Handle clicks
						_key.click(function() {
							var sub = $(this).closest('.jsonpainter-item').children('ul:first, ol:first');
							var visible = sub.is(':visible');

							// Toggle the data
							$(this).find('u').remove();
							if(visible) {
								$(this).append('\n<u>...</u>\n');
								mSettings.animate ? sub.slideUp('fast') : sub.hide();
							} else {
								mSettings.animate ? sub.slideDown('fast') : sub.show();
							}

							// Show the proper sign
							$(this).find('i').html(visible ? '+&nbsp;' : '-&nbsp;');
						});
						_key.addClass('jsonpainter-key-collapsable');
					}

					// Create and populate content
					var content = $('<' + map[0] + ' class="jsonpainter-object">').appendTo(item);
					buildNode(value, content);

					// Closing parenthesis and optional comma after it
					item.append(map[2]);

				} else {

					// Get the type of the value
					var tag = 'span';
					var type = 'null';
					if(value != null) {
						var type = typeof value;

						// For links
						if(value.toString().match(/.+:\/\/.+/)) {
							tag = 'a';
							type = 'url';
						}

						// For booleans
						if(type == 'boolean') {
							type += ' jsonpainter-value-boolean-' + value.toString();
						}
					}

					// Print value
					item.append($('<' + tag + ' class="jsonpainter-value jsonpainter-value-'+ type + '"' + (type == 'url' ? ' href="' + value + '" target="_blank"' : '') + '>').text(value));
				}

				if(++i < 1*length) item.append(',');
			}
		}

		// Default hading or errors in parsing
		function defaultOnParseError(data, element, exception) {
			element.text("Error occured while parsing JSON: " + (typeof exception == 'object' ? exception.message : exception));
		}
	}
}(jQuery));