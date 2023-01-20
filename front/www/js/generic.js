// assign to a form to avoid submit on enter press
function checkEnter(e){
	e = e || event;
	var txtArea = /textarea/i.test((e.target || e.srcElement).tagName);
	return txtArea || (e.keyCode || e.which || e.charCode || 0) !== 13;
}

function queryData(query, parent){
	console.log("Querying data...");
	$.getJSON('./ajax/' + query, (data) => {
		// parse JSON data to div
		populateFields(parent, data);
		console.log( "Done." );
	})
	.fail(function() {
		console.log("Query failed.");
	});		
}

function populateFields(parent, data) {
  // populates children in a parent object with values from data
  // name of children need to match with keys of data, values will be the values belonging to that key

  $.each(data, function(key, value){

      // sanitizing of value can be done here
    
      // process values
      var $ctrl = $('[name=' + key + ']', parent); 

      if($ctrl.is('select')){
          $("option",$ctrl).each(function(){
        if (this.value==value){
          this.selected=true;
        }else{
          this.selected=false;
        }
          });

      } else if($ctrl.is('input')){

          switch($ctrl.attr("type"))  
          {  
              case "text" :   case "hidden":  case "textarea":
          if(typeof(value) == "string"  || value == ""){
            $ctrl.val(value);
          }else{
                    $ctrl.val(Number(value).toFixed(2));
          }
                  break;
          
              case "radio" :
                  $ctrl.each(function(){
              if($(this).attr('value') == value){
                $(this).prop("checked",true);
              }else{
                $(this).prop("checked",false);
              }
          });
                  break;
          
        case "checkbox":   
                  $ctrl.each(function(){
              if($(this).attr('value') == value) {
                 $(this).prop("checked",value);
              }else{
                 $(this).prop("checked",false);
              }						   
            });  
                  break;
          } 
      } else if($ctrl.is('span')){
      if(typeof(value) == "string" || value == ""){
        $ctrl.html(value);
      }else{
        $ctrl.html(Number(value).toFixed(2));   
      }
    }
  });  
}

function renderPage(url){
  $.get(url, (data) => {
    $("#render").html(data);
  });
}