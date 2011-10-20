var fs = require('fs')
var ometa = require('./ometajs.js')

// Loads the specified grammar file and returns the generated grammar JavaScript
// code for it. Evaling this code in your module will create the grammar objects
// in the global module namespace.
exports.load = function(grammar_file){
	var grammar = fs.readFileSync(grammar_file, 'utf-8')
	var tree = ometa.BSOMetaJSParser.matchAll(grammar, 'topLevel', undefined, exports.pretty_error_handler("Error while parsing OMeta grammar"));
	var parserString = ometa.BSOMetaJSTranslator.match(tree, 'trans', undefined, exports.pretty_error_handler("Error while translating grammar"));
	
	// The evaled code defines a global object for each grammar
	return parserString
}


// Just a stupid string repeater function
var repeat_str = function(str, count){
	var list = []
	for (var i = 0; i < count; i++)
		list.push(str)
	return list.join('')
}

// Shows the exact error location and code where the OMeta parser failed to
// match.
exports.pretty_error_handler = function(error_description, line_prefix){
	if (!line_prefix)
		line_prefix = ''
	
	return function(state, index){
		var characters = state.input.lst, line_no = 1, line_start = 0, line_end = characters.length
		
		// Calculate the line number of the error and remember the last line start
		for (var i = 0; i < index && i < characters.length; i++){
			if (characters[i] == '\n'){
				line_no++
				line_start = i+1
			}
		}
		
		// Search for the end of line
		for (var i = line_start; i < characters.length; i++){
			if (characters[i] == '\n'){
				line_end = i
				break
			}
		}
		
		var line_pos = index - line_start
		var error_msg = line_prefix + error_description + '\n'
		error_msg += line_prefix + 'line ' + line_no + ' at pos ' + line_pos + ':\n'
		error_msg += line_prefix + characters.slice(line_start, line_end) + '\n'
		error_msg += line_prefix + repeat_str(' ', line_pos) + '^'
		
		throw error_msg
	}
}


// Injects a trace into the specified OMeta object. Every applied rule will be
// shown on the console.
exports.inject_tracer = function(grammar_object){
  
  var tracer_level = 0
  var tracer_last_idx = 0
  
  var level_prefix = function(level){
  	return repeat_str('  ', level)
  }
  
  var tracer = function(original_func, rule_prefix){
		return function(rule){
			if (rule != 'anything'){
				var arg_list = Array.prototype.slice.call(arguments, 1);
				if (arg_list.length > 0)
					console.error( level_prefix(tracer_level) + rule_prefix + rule + '(' + arg_list.join(', ') + ')' );
				else
					console.error( level_prefix(tracer_level) + rule_prefix + rule );
			}
		  
		  tracer_level++;
		  try {
		    var result = original_applyWithArgs.apply(this, arguments);
		  } finally {
		    tracer_level--;
		    
		    /*
		    var consumed = this.input.idx > tracer_last_idx, matched = !!result
		    if (consumed || matched){
		    	var report_msg = [ level_prefix(tracer_level), '→' ]
		    	if (consumed)
		    		report_msg.push( ' consumed "' + this.input.lst.slice(tracer_last_idx, this.input.idx) + '"' )
		    	if (matched)
		    		report_msg.push( ' matched: ' + result )
		    	console.error(report_msg.join(''))
		    	
		    	tracer_last_idx = this.input.idx
		    }
		    */
		  }
		  
		  return result;
		}
	}
	
	var original_apply = grammar_object._apply
	grammar_object._apply = tracer(original_apply, '')
	
	var original_applyWithArgs = grammar_object._applyWithArgs
	grammar_object._applyWithArgs = tracer(original_applyWithArgs, '')
	
	var original_superApplyWithArgs = grammar_object._superApplyWithArgs
	grammar_object._superApplyWithArgs = tracer(original_superApplyWithArgs, '^')
	
	// We inject code to reset the tracer level and index before each match
	var original_genericMatch = grammar_object._genericMatch;
	grammar_object._genericMatch = function(){
		tracer_level = 0;
		tracer_last_idx = 0;
		return original_genericMatch.apply(this, arguments);
	}
	
}
