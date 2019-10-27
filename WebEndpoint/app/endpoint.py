#!/usr/bin/env python
# coding=utf-8

try:
	# Python 2.x
	from SocketServer import ThreadingMixIn
	from SimpleHTTPServer import SimpleHTTPRequestHandler
	from BaseHTTPServer import HTTPServer, BaseHTTPRequestHandler
	from urllib import unquote
	from urlparse import urlparse, parse_qs
except ImportError:
	# Python 3.x
	from socketserver import ThreadingMixIn
	from http.server import SimpleHTTPRequestHandler, HTTPServer, BaseHTTPRequestHandler
	from urllib.parse import urlparse, unquote, parse_qs

from datetime import datetime, timedelta
import sys
import os
import posixpath
import json
import math

def agg_last(v1,v2):
	return v2

def agg_max(v1,v2):
	if v1>v2:
		return v1
	return v2

def agg_min(v1,v2):
	if v1<v2:
		return v1
	return v2

def num(v,digits=1):
	if v is None:
		return "---"
	return ('{0:.%sf}'%digits).format(v).replace('.',',')

# https://stackoverflow.com/questions/13071384/python-ceil-a-datetime-to-next-quarter-of-an-hour
def ceil_dt(dt):
    # how many secs have passed this hour
    nsecs = dt.minute*60 + dt.second + dt.microsecond*1e-6  
    # number of seconds to next quarter hour mark
    # Non-analytic (brute force is fun) way:  
    #   delta = next(x for x in xrange(0,3601,900) if x>=nsecs) - nsecs
    # analytic way:
    delta = math.ceil(nsecs / 900) * 900 - nsecs
    #time + number of seconds to quarter hour mark.
    return dt + timedelta(seconds=delta)


# http://snowfence.umn.edu/Components/winddirectionanddegreeswithouttable3.htm
def wdir(v):
	if v is None:
		return ""
	if v>348.75 or v<11.25: 
		return 'N'
	if v<33.75:
		return 'NNO'
	if v<56.25:
		return 'NO'
	if v<78.75:
		return 'ONO'
	if v<101.25:
		return 'O'
	if v<123.75:
		return 'OSO'
	if v<146.25:
		return 'SO'
	if v<168.75:
		return 'SSO'
	if v<191.25:
		return 'S'
	if v<213.75:
		return 'SSW'
	if v<236.25:
		return 'SW'
	if v<258.75:
		return 'WSW'
	if v<281.25:
		return 'W'
	if v<303.75:
		return 'WNW'
	if v<326.25:
		return 'NW'
	if v<348.75:
		return 'NNW'
	return "???"

def wdirchar(v):
	if v is None:
		return None
	# Pfeil 180 grad drehen -> Windrichtung
	v=(v+180)%360
	if v>337.5 or v<22.5: 
		return '↑'
	if v<67.5:
		return '↗'
	if v<112.5:
		return '→'
	if v<157.5:
		return '↘'
	if v<202.5:
		return '↓'
	if v<247.5:
		return '↙'
	if v<146.25:
		return '←'
	if v<292.5:
		return '↖'
	return None

def to_knots(kmh):
	if kmh is None:
		return None
	return kmh/1.852


def json_serial(obj):
	"""JSON serializer for objects not serializable by default json code"""

	if isinstance(obj, datetime):
		return obj.strftime('%Y-%m-%d %H:%M:%S')
	raise TypeError ("Type %s not JSON serializable" % type(obj))

class ThreadingSimpleServer(ThreadingMixIn, HTTPServer):
	def __init__(self, base_path, datadir, api_key, *args, **kwargs):
		HTTPServer.__init__(self, *args, **kwargs)
		self.base_path = base_path
		self.datafile = os.path.join(datadir,'values.json')
		self.api_key = api_key
		self.sensordata={}
		self.values=self.read_values()

	def read_values(self):
		values=[]
		if os.path.isfile(self.datafile):
			with open(self.datafile,'r') as f:
				for line in f:
					v=json.loads(line)
					v['time']=datetime.strptime(v['time'],'%Y-%m-%d %H:%M:%S')
					values.append(v)

		return values

	def append_values(self, values):
		self.values.append(values)
		# 300 Werte ist etwas mehr als 24h bei 5 Minuten-Werten
		if len(self.values)>300:
			self.values=self.values[-300:]
		with open(self.datafile,'w') as f:
			for value in self.values:
				f.write(json.dumps(value,default=json_serial))
				f.write('\n')

	def get_value(self,key,ts_start,ts_end,aggregate):
		result = None
		for v in self.values:
			if v['time']<ts_start:
				continue
			if v['time']>ts_end:
				continue
			if not key in v:
				continue
			val=v[key]
			if val is None:
				continue
			if result is None:
				result=val
			else:
				result=aggregate(result,val)
		return result


class PathBoundHttpRequestHandler(SimpleHTTPRequestHandler):
	# http://tiao.io/posts/python-simplehttpserver-recipe-serve-specific-directory/
	def translate_path(self, path):
		path = posixpath.normpath(unquote(path))
		words = path.split('/')
		words = filter(None, words)
		path = self.server.base_path
		for word in words:
			drive, word = os.path.splitdrive(word)
			head, word = os.path.split(word)
			if word in (os.curdir, os.pardir):
				continue
			path = os.path.join(path, word)
		return path

class CustomHttpRequestHandler(PathBoundHttpRequestHandler):
	def __init__(self, *args, **kwargs):
		PathBoundHttpRequestHandler.__init__(self, *args, **kwargs)

	def do_GET(self):
		url=urlparse(self.path)
		if url.path=='/publish':
			self.handle_publish(parse_qs(url.query))
		elif url.path=='/metrics':
			self.print_metrics()
		elif url.path=='/':
			self.print_status()
		else:
			PathBoundHttpRequestHandler.do_GET(self)

	def print_metrics(self):
		start=datetime.now()
		end=start-timedelta(minutes=15)

		data={}
		data['surfdata_wind_min']=self.server.get_value('wma',end,start,agg_min);
		data['surfdata_wind_avg']=self.server.get_value('wav',end,start,agg_max);
		data['surfdata_wind_max']=self.server.get_value('wav',end,start,agg_max);
		data['surfdata_wind_dir']=self.server.get_value('wd',end,start,agg_last);
		data['surfdata_t_luft']=self.server.get_value('t',end,start,agg_max);
		data['surfdata_h_luft']=self.server.get_value('h',end,start,agg_max);
		data['surfdata_t_wasser']=self.server.get_value('xt',end,start,agg_max);
		data['v_capacitor']=self.server.get_value('vc',end,start,agg_last);
		data['v_battery']=self.server.get_value('xv',end,start,agg_last);
		data['sun']=self.server.get_value('vs',end,start,agg_last);

		text=""
		for (k,v) in data.items():
			if v is not None:
				text+="%s %s\n"%(k,v)

		self.send_text_response(text,content_type="text/plain")

	def print_status(self):

		with open(os.path.join(self.server.base_path,'index.html'),'r') as f:
			html=str(f.read())

		now=datetime.now()
		t30=now-timedelta(minutes=30)
		t120=now-timedelta(minutes=120)

		chartstep=timedelta(minutes=15)
		chart_end_ts=ceil_dt(now)
		# noch keine Daten in der aktuellen Viertelstunde -> diese noch nicht mit ausgeben
		if self.server.values[-1]['time']<(chart_end_ts-chartstep):
			chart_end_ts=chart_end_ts-chartstep

		chart_start_ts=chart_end_ts-timedelta(hours=12)

		chartdata={
			"labels": [],
			"wind_min": [],
			"wind_dir": [],
			"wind_avg": [],
			"wind_max": [],
			"t_luft": [],
			"t_wasser": [],
		}
		while chart_start_ts<chart_end_ts:
			start=chart_start_ts
			end=chart_start_ts=chart_start_ts+chartstep
			chartdata['labels'].append(start.strftime('%H:%M'))
			chartdata['wind_min'].append(self.server.get_value('wmi',start,end,agg_min))
			chartdata['wind_avg'].append(self.server.get_value('wav',start,end,agg_max))
			chartdata['wind_max'].append(self.server.get_value('wma',start,end,agg_max))
			chartdata['wind_dir'].append(wdirchar(self.server.get_value('wd',start,end,agg_last)))
			chartdata['t_luft'].append(self.server.get_value('t',start,end,agg_max))
			chartdata['t_wasser'].append(self.server.get_value('xt',start,end,agg_max))

		wind_dir=self.server.get_value('wd',t30,now,agg_last)
		html=html.format(
			now=now,
			t_luft=num(self.server.get_value('t',t30,now,agg_last)),
			t_wasser=num(self.server.get_value('xt',t120,now,agg_last)),
			wind_avg=num(self.server.get_value('wav',t30,now,agg_min),0),
			wind_max=num(self.server.get_value('wma',t30,now,agg_max),0),
			wind_dir=wind_dir,
			wind_dir_name=wdir(wind_dir),
			chartdata=json.dumps(chartdata)
			);

		self.send_text_response(html)
		return

	def handle_publish(self,params):
		if not self.check_apikey(params):
			return
		now=datetime.now()
		values={}
		values['time']=now
		if 'wmi' in params:
			# Wind min - km/h
			values['wmi']=float(params['wmi'][0])
			self.server.sensordata['wmi']={
				't':now,
				'v':params['wmi'][0],
				'l':'Wind min',
				'u':'km/h'
				}
		if 'wma' in params:
			values['wma']=float(params['wma'][0])
			# Wind max - km/h
			self.server.sensordata['wma']={
				't':now,
				'v':params['wma'][0],
				'l':'Wind max',
				'u':'km/h'
				}
		if 'wav' in params:
			values['wav']=float(params['wav'][0])
			# Wind avg - km/h
			self.server.sensordata['wav']={
				't':now,
				'v':params['wav'][0],
				'l':'Wind avg',
				'u':'km/h'
				}
		if 'wd' in params:
			values['wd']=float(params['wd'][0])
			# Wind direction °
			self.server.sensordata['wd']={
				't':now,
				'v':params['wd'][0],
				'l':'Wind direction',
				'u':'°'
				}
		if 'wg' in params:
			values['wg']=float(params['wg'][0])
			# Wind gust - km/h
			self.server.sensordata['wg']={
				't':now,
				'v':params['wg'][0],
				'l':'Wind gust',
				'u':'km/h'
				}
		if 't' in params:
			values['t']=float(params['t'][0])
			# Temperature outside - °C
			self.server.sensordata['t']={
				't':now,
				'v':params['t'][0],
				'l':'Temperature outside',
				'u':'°C'
				}
		if 'h' in params:
			values['h']=float(params['h'][0])
			# Humidity outside - °C
			self.server.sensordata['h']={
				't':now,
				'v':params['h'][0],
				'l':'Humidity outside outside',
				'u':'%'
				}
		if 'r' in params:
			values['r']=float(params['r'][0])
			# Rain - mm/h
			self.server.sensordata['r']={
				't':now,
				'v':params['r'][0],
				'l':'Rain',
				'u':'mm/h'
				}
		if 'vc' in params:
			values['vc']=float(params['vc'][0])
			# Capacitor - V
			self.server.sensordata['vc']={
				't':now,
				'v':params['vc'][0],
				'l':'Capacitor',
				'u':'V'
				}
		if 'vs' in params:
			values['vs']=float(params['vs'][0])
			# Solar - V
			self.server.sensordata['vs']={
				't':now,
				'v':params['vs'][0],
				'l':'Solar',
				'u':'V'
				}
		if 'xt' in params:
			values['xt']=float(params['xt'][0])
			# External Sensor Temperature - °C
			self.server.sensordata['xt']={
				't':now,
				'v':params['xt'][0],
				'l':'External Sensor Temperature',
				'u':'°C'
				}
		if 'xv' in params:
			values['xv']=float(params['xv'][0])
			# External Sensor Battery - V
			self.server.sensordata['xv']={
				't':now,
				'v':params['xv'][0],
				'l':'External Sensor Battery',
				'u':'V'
				}
		self.send_text_response('OK\n')
		self.server.append_values(values)

	def check_apikey(self,params):
		if not self.server.api_key:
			# no api key set
			return True
		if 'api_key' in params and params['api_key'][0]==self.server.api_key:
			# correct api key provided 
			return True
		self.send_text_response('api_key not accepted\n',403)
		return False

	def send_text_response(self,text,status=200,content_type="text/html;charset=utf-8"):
		try:
			text=text.encode()
		except:
			pass
		self.send_response(status)
		self.send_header("Content-type", content_type)
		self.send_header("Content-length", len(text))
		self.end_headers()
		self.wfile.write(text)


def main():
	if 'API_KEY' in os.environ:
		print("Using API_KEY from environment")
		api_key=os.environ['API_KEY']
	else:
		#print("No API_KEY set - anyone can post data to this endpoint!")
		#api_key=None
		api_key="secret"
	webroot=os.path.join(os.path.dirname(os.path.realpath(__file__)),'static')
	datadir=os.path.join(os.path.dirname(os.path.realpath(__file__)),'data')
	server = ThreadingSimpleServer(webroot,datadir,api_key, ('', 8080), CustomHttpRequestHandler)
	try:
		os.makedirs(datadir)
	except OSError:
		pass

	print("Started server on port 8080")
	try:
		while 1:
			sys.stdout.flush()
			server.handle_request()
	except KeyboardInterrupt:
		print('Stopped.')

if __name__ == "__main__":
	main()
