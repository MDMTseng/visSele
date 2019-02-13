

const EARTH_RADIOUS_m = 6371008;
const DEG2RAD = Math.PI/180;
//Volumetric mean radius (km)
export function GeoCoordToXY_approx(lat_o,lon_o) {//in short range aproximate

  let lon_or = lon_o*DEG2RAD;
  let lat_or = lat_o*DEG2RAD;

  //console.log("lat_o:"+lat_o+" cos(lat_o):"+Math.cos(lat_or));
  return {
    x:Math.cos(lat_or)*lon_or*EARTH_RADIOUS_m,
    y:lat_or*EARTH_RADIOUS_m
  };
}

export function GeoDiffToXY_approx(lat_O,lon_O,lat,lon) {//in short range aproximate

  let lat_Or = lat_O*DEG2RAD;
  let lon_Or = lon_O*DEG2RAD;
  let lat_r = lat*DEG2RAD;
  let lon_r = lon*DEG2RAD;
  return {
    x:Math.cos(lat_Or)*(lon_r-lon_Or)*EARTH_RADIOUS_m,
    y:(lat_r-lat_Or)*EARTH_RADIOUS_m
  };
}
/*
var corLonLats=[
  {
    lon:20,
    lat:80
  },
  {
    lon:22.1,
    lat:82
  },
];

var ccorXY=corLonLats.map(coor=>GeoCoordToXY_approxy(coor.lon,coor.lat));

console.log("coordinates::");
console.log(JSON.stringify(corLonLats,0,0));
console.log("Dist>>"+Math.hypot(ccorXY[1].x-ccorXY[0].x,ccorXY[1].y-ccorXY[0].y));
console.log("Actan>>"+Math.atan2(ccorXY[1].x-ccorXY[0].x,ccorXY[1].y-ccorXY[0].y)/DEG2RAD);
*/
