from http.server import BaseHTTPRequestHandler, HTTPServer
import ctypes

class RequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/trigger':
            ctypes.windll.user32.keybd_event(0x5B, 0, 0, 0)  # Press Win keys
            ctypes.windll.user32.keybd_event(0x44, 0, 0, 0)  # Press D key
            ctypes.windll.user32.keybd_event(0x44, 0, 2, 0)  # Release D key
            ctypes.windll.user32.keybd_event(0x5B, 0, 2, 0)  # Release Win key
            
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.end_headers()
            self.wfile.write(b'Triggered Win+D')
        else:
            self.send_response(404)
            self.end_headers()

def run(server_class=HTTPServer, handler_class=RequestHandler, port=5000):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    print(f'Starting httpd on port {port}...')
    httpd.serve_forever()

if __name__ == "__main__":
    run()
