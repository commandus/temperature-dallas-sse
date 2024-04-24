# Dallas

```html
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>SSE demo</title>
</head>
<body>
<script>
const ev1 = new EventSource("/sse/event1");
ev1.onmessage = function(e) {
  console.log('ev1', e.data);
}
const ev2 = new EventSource("/sse/event2");
ev2.onmessage = function(e) {
  console.log('ev2', e.data);
}
</script>
</body>
</html>
```

## Configure nginx proxy

```sh
sudo vi /opt/xray/config.json
sudo systemctl restart xray
sudo certbot certonly --standalone --preferred-challenges http -d lora.commandus.com
sudo vi /etc/letsencrypt/renewal/t.commandus.com.conf
    add line: renew_hook = systemctl restart xray
vi /home/andrei/renew-certificate.sh
```

## Build server

Run docker Ubuntu 18

Build
```
cd src/temperature-dallas-sse
cd build-ubuntu-18
cmake ..
make
strip temperature-dallas-sse
```

## Host HTML page

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>SSE demo</title>
</head>
<body>
<script>
  const ev = new EventSource("/sse/event");
  ev.onmessage = function(e) {
    const d = e.data;
    console.log(new Date(), d);
    const l = document.getElementById("t");
    const n = document.createElement("div");
    const t = document.createTextNode(d);
    n.appendChild(t);
    l.appendChild(n);
  }
</script>
  <a target="_blanc" href="https://t.commandus.com/sse/send?a=1&b=Ab">Send</a>  
<p id="t">
    
</p>  
</body>
</html>
```

## Copy binary
```
scp temperature-dallas-sse andrei@lora.commandus.com:~/temperature-dallas-sse/ 
```

## Start SSE server
```sh
ssh andrei@lora.commandus.com
cd ~/temperature-dallas-sse
nohup ./temperature-dallas-sse &
```

```shell
wget -q -O - "https://t.commandus.com/sse/send?a=1&b=Ab"
``` 

or

```shell
 wget -q -O - "http://localhost:1234/send?a=1&b=Ab"
```

### nginx proxy

```
server {
        listen 127.0.0.1:8084;
        listen [::1]:8084 default_server;
        root /var/www/html/t;
        index index.html index.htm;
        server_name t.commandus.com;
        location / {
                try_files $uri $uri/ =404;
        }
        location /sse {
                rewrite /sse(/.*) $1 break;
                proxy_pass http://127.0.0.1:1234;
                proxy_buffering off;
                proxy_cache off;
                proxy_set_header Host $host;
                proxy_set_header Connection '';
                proxy_http_version 1.1;
                chunked_transfer_encoding off;
                # prevents 502 bad gateway error
                proxy_buffers 8 32k;
                proxy_buffer_size 64k;
                reset_timedout_connection on;
                proxy_read_timeout 24h;
        }
}
```

### HTML page

```html

<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>SSE demo</title>
</head>
<body>
<script>
  const ev = new EventSource("/sse/event");
  ev.onmessage = function(e) {
    const d = e.data;
    console.log(new Date(), d);
    const l = document.getElementById("t");
    const n = document.createElement("div");
    const t = document.createTextNode(d);
    n.appendChild(t);
    l.appendChild(n);
  }
</script>
  <a target="_blanc" href="https://t.commandus.com/sse/send?a=1&b=Ab">Send</a>  
<p id="t">
    
</p>  
</body>
</html>
```
