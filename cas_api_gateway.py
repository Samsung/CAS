import argparse
import logging
import json
from os import path
from typing import Dict, Tuple
from flask_cors import CORS
from gevent.pywsgi import WSGIServer
from flask import Blueprint, Flask, Response, render_template_string, request, stream_with_context
import requests
from gevent.pool import Pool

template="""
<!doctype html>
<html>
<head>
    <title>Database List</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet" integrity="sha384-9ndCyUaIbzAi2FUVXJi0CjmCapSmO7SnpJef0486qhLnuZ2cdeRhO02iuK6FUUVM" crossorigin="anonymous">
    <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/bootstrap-icons@1.11.3/font/bootstrap-icons.min.css">
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js" integrity="sha384-geWF76RCwLtnZ8qwWowPQNguL3RmwHVBC9FhGdlKrxdiJJigb/j/68SIy3Te4Bkz" crossorigin="anonymous"></script>
</head>
<body>
<div id="content">
<div class="text-center" style="display:flex;justify-content: center">
    <table class="table table-hover table-bordered align-middle table-responsive-sm" style="width: fit-content;padding: 20px">
        <thead class="table-dark">
        <tr>
            <th scope="col">DB name</th>
            <th scope="col">Host</th>
            <th scope="col">Image<br/>version</th>
            <th scope="col" style="width: 70px;">config<br/>file</th>
            <th scope="col" style="width: 70px;">config<br/>loaded</th>
            <th scope="col" style="width: 70px">nfsdb<br/>file</th>
            <th scope="col" style="width: 70px">nfsdb<br/>loaded</th>
            <th scope="col" style="width: 70px;">deps<br/>loaded</th>
        </tr>
        </thead>
        <tbody>
        {% for host, val in backend.items() %}
          {% if val is not none %}
            {% for endpoint, v in val.items() %}
            {% if engine != v['image_version'] %}
            <tr class="table-danger" title="Image version not compatibile with engine version!">
            {%else%}
            <tr>
            {%endif%}
                <td class="text-start fw-bolder"><a href='{{ web_ctx }}{{ endpoint }}'>{{ endpoint }}</a></td>
                <td class="text-center">{{ v['host'] }}</td>
                <td class="text-center">{{ v['image_version'] }}</td>
                <td class="text-center"><i class="bi {{ "bi-check-circle-fill" if v['config_path']  else "bi-circle" }}"></i></td>
                <td class="text-center"><i class="bi {{ "bi-check-circle-fill" if v['loaded_config'] else "bi-circle" }}"></i></td>
                <td class="text-center"><i class="bi {{ "bi-check-circle-fill" if v['nfsdb_path'] else "bi-circle" }}"></i></td>
                <td class="text-center"><i class="bi {{ "bi-check-circle-fill" if v['loaded_nfsdb'] else "bi-circle" }}"></i></td>
                <td class="text-center"><i class="bi {{ "bi-check-circle-fill" if v['deps_path'] else "bi-circle" }}"></i></td>
            </tr>
            {% endfor %}
          {% endif %}
        {% endfor %}
        </tbody>
    </table>

</div>
{% for err in not_responding %}
<div class="alert alert-warning" role="alert">
    <strong>{{err}}</strong>
</div>
{% endfor %}
</div>
<div id="footer">

</div>
</body>
</html>
"""

script_dir = path.dirname(path.realpath(__file__))
config_file = path.join(script_dir,'gw_config.json')
cas_gw = Blueprint('cas_gateway', __name__)
host = "0.0.0.0"
port = 8080
ctx = "/"
conf = {}
engine_version = 4

BACKEND_SERVERS_STATUS = {}
if path.exists(config_file):
    with open(config_file, 'r', encoding="utf-8") as conffile:
        conf = json.load(conffile)
p = Pool(len(conf["BACKEND_SERVERS"]))

def get_host_from_endpoint(endpoint):
    matched = [ host for host,endp in conf["BACKEND_SERVERS"].items() if endp == endpoint]
    if len(matched) == 1:
        return matched[0]
    else:
        return None

def generate_stream(source_response):
    for chunk in source_response.iter_content(chunk_size=8192):
        yield chunk

def get_response(host_info: Tuple) -> Tuple[str, Dict | None]:
    try:
        host, url = host_info
        return (host, requests.get(url, timeout=0.2).json())
    except Exception:
        return (host, None)

def get_status():
    req = [(host, f"{addr}/status/") for host, addr in conf["BACKEND_SERVERS"].items()]
    ret = {}
    rsp = p.map_async(get_response, req).get(True)
    for r in rsp:
        if r[1] is not None:
            ret[conf["BACKEND_SERVERS"][r[0]]] = {}
            for x in r[1]:
                tmp = r[1][x]
                tmp["host"] = r[0]
                ret[conf["BACKEND_SERVERS"][r[0]]][x] = tmp
        else:
            ret[conf["BACKEND_SERVERS"][r[0]]] = None
    return ret

@cas_gw.route('/status/',methods=['GET'], strict_slashes=False)
def status():
    return Response(json.dumps( get_status(), indent=2),
        mimetype="application/json"
    )

@cas_gw.route('/<db>/', defaults={'cmd': ""}, methods=['GET', 'POST'])
@cas_gw.route('/<db>/<cmd>', methods=['GET', 'POST'])
def proxy(db, cmd):
    global BACKEND_SERVERS_STATUS
    endpoint = [ e for e in BACKEND_SERVERS_STATUS if BACKEND_SERVERS_STATUS[e] is not None and db in BACKEND_SERVERS_STATUS[e]]
    if len(endpoint) == 0:
        BACKEND_SERVERS_STATUS = get_status()
        endpoint = [ e for e in BACKEND_SERVERS_STATUS if BACKEND_SERVERS_STATUS[e] is not None and db in BACKEND_SERVERS_STATUS[e]]

    if len(endpoint) == 1:
        try:
            url = f"{endpoint[0]}/{request.full_path.replace(ctx,'')}"
            method = request.method
            headers = {'Content-Type': request.headers.get('Content-Type')}
            data = request.get_json() if method in ['POST', 'PUT'] else None
            source_response = requests.request(method, url, headers=headers, json=data, stream=True, timeout=2000)
            # print (url)
            return Response(stream_with_context(generate_stream(source_response)), content_type=source_response.headers['Content-Type'])
        except requests.exceptions.ConnectionError as e:
            return Response(json.dumps({"ERROR": str(e)}), mimetype="application/json", status=500)
    else:
        return Response(json.dumps({"ERROR": "Database not found!"}), mimetype="application/json", status=500)

@cas_gw.route('/', methods=['GET', 'POST'])
def main():
    backends = get_status()
    not_responding = [ f"WARNING: {get_host_from_endpoint(x)} is not responding! " for x in backends if backends[x] is None ]
    return Response(render_template_string(template, backend=backends, servers=conf["BACKEND_SERVERS"],
                                            not_responding=not_responding,engine=engine_version, web_ctx=""))

if __name__ == '__main__':

    arg_parser = argparse.ArgumentParser(description="CAS Gateway arguments")
    arg_parser.add_argument("--port", "-p", type=int, default=8080, help="server port")
    arg_parser.add_argument("--host", type=str, default="0.0.0.0", help="server address")
    arg_parser.add_argument("--ctx", type=str, default='/', help="Web context")
    arg_parser.add_argument("--debug", action="store_true", help="debug mode")

    a = arg_parser.parse_args()

    port = conf["PORT"] if "PORT" in conf else a.port
    host = conf["HOST"] if "HOST" in conf else a.host
    ctx = conf["WEB_CONTEXT"] if "WEB_CONTEXT" in conf else a.ctx

    ctx = ctx if ctx.endswith("/") else ctx + "/"
    try:
        app = Flask(__name__, template_folder='client/templates', static_folder='client/static', static_url_path=path.join(ctx, 'static'))
        CORS(app)
        app.register_blueprint(cas_gw, url_prefix=ctx)
        if a.debug:
            print(app.url_map)
        http_server = WSGIServer((host, port), app, log="default" if a.debug else None)
        print("Started CAS API Gateway")
        http_server.serve_forever()
    except KeyboardInterrupt:
        print("Shutting down CAS API Gateway")