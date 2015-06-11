#!/usr/bin/env python
#
# Copyright 2014-present Facebook. All Rights Reserved.
#
# This program file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program in a file named COPYING; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor,
# Boston, MA 02110-1301 USA
#


from ctypes import *
from bottle import route, run, template, request, response, ServerAdapter
from bottle import abort
from wsgiref.simple_server import make_server, WSGIRequestHandler, WSGIServer
import json
import ssl
import socket
import os
from rest_fruid import *
from rest_server import *
from rest_sensors import *
from rest_bmc import *
from rest_gpios import *
from rest_modbus import *

CONSTANTS = {
    'certificate': '/usr/lib/ssl/certs/rest_server.pem',
}

# Handler for root resource endpoint
@route('/api')
def rest_api():
   result = {
                "Information": {
                    "Description": "Wedge RESTful API Entry",
                    },
                "Actions": [],
                "Resources": [ "sys"],
             }

   return result

# Handler for sys resource endpoint
@route('/api/sys')
def rest_sys():
    result = {
                "Information": {
                    "Description": "Wedge System",
                    },
                "Actions": [],
                "Resources": [ "mb", "bmc", "server", "sensors", "gpios"],
             }

    return result

# Handler for sys/mb resource endpoint
@route('/api/sys/mb')
def rest_sys():
    result = {
                "Information": {
                    "Description": "System Motherboard",
                    },
                "Actions": [],
                "Resources": [ "fruid"],
             }

    return result

# Handler for sys/mb/fruid resource endpoint
@route('/api/sys/mb/fruid')
def rest_fruid():
  return get_fruid()

# Handler for sys/bmc resource endpoint
@route('/api/sys/bmc')
def rest_bmc():
    return get_bmc()

# Handler for sys/server resource endpoint
@route('/api/sys/server')
def rest_bmc():
    return get_server()

# Handler for uServer resource endpoint
@route('/api/sys/server', method='POST')
def rest_server():
    data = json.load(request.body)
    return server_action(data)

# Handler for sensors resource endpoint
@route('/api/sys/sensors')
def rest_sensors():
  return get_sensors()

# Handler for sensors resource endpoint
@route('/api/sys/gpios')
def rest_gpios():
  return get_gpios()

@route('/api/sys/modbus_registers')
def modbus_registers():
    return get_modbus_registers()

run(host = "::", port = 8080)

# SSL Wrapper for Rest API
class SSLWSGIRefServer(ServerAdapter):
    def run(self, handler):
        if self.quiet:
            class QuietHandler(WSGIRequestHandler):
                def log_request(*args, **kw): pass
            self.options['handler_class'] = QuietHandler

        # IPv6 Support
        server_cls = self.options.get('server_class', WSGIServer)

        if ':' in self.host:
            if getattr(server_cls, 'address_family') == socket.AF_INET:
                class server_cls(server_cls):
                    address_family = socket.AF_INET6

        srv = make_server(self.host, self.port, handler,
                server_class=server_cls, **self.options)
        srv.socket = ssl.wrap_socket (
                srv.socket,
                certfile=CONSTANTS['certificate'],
                server_side=True)
        srv.serve_forever()

# Use SSL if the certificate exists. Otherwise, run without SSL.
if os.access(CONSTANTS['certificate'], os.R_OK):
    run(server=SSLWSGIRefServer(host="::", port=8443))
else:
    run(host = "::", port = 8080)
