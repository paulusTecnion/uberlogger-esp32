getStatus();

setInterval(getStatus, 2000);

function getStatus(){
  queryData('getStatus', '#status');
}