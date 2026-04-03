# Hosting de la web de descargas

## Opcion recomendada
Como esta web es estatica, lo mas simple es servirla con Nginx en tu servidor Debian.

## Estructura recomendada en el servidor

```text
/var/www/kmn-space/
├── index.html
├── styles.css
├── app.js
├── data/
│   └── releases.json
└── downloads/
    ├── 0.0.3/
    │   ├── KMN-Space-Setup-0.0.3.exe
    │   └── KMN-Space-0.0.3.dmg
    └── 0.0.4/
```

## Instalar Nginx

```bash
sudo apt update
sudo apt install nginx
```

## Copiar la web

```bash
sudo mkdir -p /var/www/kmn-space
sudo cp -R website/* /var/www/kmn-space/
```

## Configuracion de Nginx
Crea `/etc/nginx/sites-available/kmn-space` con:

```nginx
server {
    listen 80;
    listen [::]:80;
    server_name tu-dominio.com www.tu-dominio.com;

    root /var/www/kmn-space;
    index index.html;

    location / {
        try_files $uri $uri/ =404;
    }

    location /downloads/ {
        add_header Cache-Control "public, max-age=300";
        try_files $uri =404;
    }
}
```

Despues:

```bash
sudo ln -s /etc/nginx/sites-available/kmn-space /etc/nginx/sites-enabled/kmn-space
sudo nginx -t
sudo systemctl reload nginx
```

## Si tienes todos los puertos cerrados
Para que alguien pueda acceder desde fuera necesitas abrir al menos:
- `80/tcp` para HTTP
- `443/tcp` para HTTPS

Si usas `ufw`:

```bash
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw reload
```

Si el servidor esta detras de router o proveedor cloud, tambien debes abrir o redirigir esos mismos puertos ahi.

## HTTPS
Cuando el dominio apunte a tu servidor, puedes activar HTTPS con Let's Encrypt:

```bash
sudo apt install certbot python3-certbot-nginx
sudo certbot --nginx -d tu-dominio.com -d www.tu-dominio.com
```

## Actualizar una release
1. Sube los nuevos instaladores a `/var/www/kmn-space/downloads/<version>/`.
2. Sustituye `data/releases.json` por la nueva version.
3. No necesitas reiniciar Nginx para cambios en archivos estaticos.
