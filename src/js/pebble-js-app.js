
function iconFromWeatherId(weatherId, daynight) {
  if (weatherId < 600) {
    return 6; // rain
  } else if (weatherId == 611 || weatherId == 612) {
    return 7; // sleet
  } else if (weatherId < 700) {
    return 8; // snow
  } else if (weatherId < 800) {
    return 3; // fog
  } else if (weatherId == 801) {
    if (daynight == 'd') { 
      return 4; // partly cloudy day
    } else {
      return 5; // partly cloudy night
    }
  } else if (weatherId > 800) {
    return 2; // cloudy
  } else {
    if (daynight == 'd') { 
      return 0; // clear day
    } else {
      return 1; // clear night
    }
  }
}

var temperature = "-";
var icon;
var temperature_next = "-";
var icon_next;

function fetchWeather(latitude, longitude) {
  var req = new XMLHttpRequest();
  
  req.open('GET', "http://api.openweathermap.org/data/2.5/weather?" +
    "lat=" + latitude + "&lon=" + longitude + "&cnt=1", true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if(req.status == 200) {
        console.log(req.responseText);

        var response = JSON.parse(req.responseText);
        temperature = Math.round(response.main.temp - 273.15);
        icon = iconFromWeatherId(response.weather[0].id, response.weather[0].icon.slice(-1));
        var city = response.name;
        console.log(city);
        sendWeather();
      } else {
        console.log("Error");
      }
    }
  }
  req.send(null);

  req2 = new XMLHttpRequest();
  req2.open('GET', "http://api.openweathermap.org/data/2.5/forecast/daily?" +
    "lat=" + latitude + "&lon=" + longitude + "&cnt=2", true);
  req2.onload = function(e) {
    if (req2.readyState == 4) {
      if(req2.status == 200) {
        console.log(req2.responseText);
        
        var response = JSON.parse(req2.responseText);
        temperature_next = Math.round(response.list[1].temp.day - 273.15);
        icon_next = iconFromWeatherId(response.list[1].weather[0].id, response.list[1].weather[0].icon.slice(-1));
        var city = response.city.name;
        console.log(city);
        sendWeather();
      } else {
        console.log("Error");
      }
    }
  }
  req2.send(null);

  sendWeather();
}

function sendWeather() {
  Pebble.sendAppMessage({
    "WEATHER_TEMPERATURE_KEY":temperature + "\u00B0C",
    "WEATHER_ICON_KEY":icon,
    "WEATHER_TEMPERATURE_NEXT_KEY":temperature_next + "\u00B0C",
    "WEATHER_ICON_NEXT_KEY":icon_next}
  );
}

function locationSuccess(pos) {
  var coordinates = pos.coords;
  fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
  console.warn('location error (' + err.code + '): ' + err.message);
  Pebble.sendAppMessage({
    "WEATHER_TEMPERATURE_KEY":"N/A",
    "WEATHER_TEMPERATURE_NEXT_KEY":"N/A"
  });
}

Pebble.addEventListener("appmessage",
  function(e) {
    console.log("Received a message from the watch.");
    console.log(e.payload);
    window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError);
  }
);