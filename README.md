# Trabajo Práctico Especial | Protocolos de Comunicación

## Grupo 7 - Integrantes
- LIU, Jonathan Daniel - 62533
- MARENGO, Tomas Santiago - 61587
- VILAMOWSKI, Abril - 62495
- WISCHÑEVSKY, David - 62494


# POP3
Se implementó un servidor POP3 siguiendo los RFCS 1939 y 2449. 

# DAJT
Protocolo diseñado por el grupo para administrar el servidor POP3.

# Materiales
Dentro de la carpeta `docs` se encuentra el informe del trabajo práctico y el RFC del protocolo DAJT. 
El código fuente del servidor POP3 se encuentra dentro de `src` y el del cliente DAJT dentro de `client`. 


## Guía de instalación y uso
1. Ejecutar en la carpeta raíz del proyecto y dentro de la carpeta `client`, el comando `make all CC=<compiler>`, donde \<compiler> debe ser reemplazado por gcc o clang para compilar tanto el servidor POP3 como el cliente DAJT. Los binarios del servidor POP3 se encontrarán en la carpeta raíz, mientras que los del cliente dentro de su carpeta correspondiente.
   
2. Ejecutar los binarios con los flags deseados. Ejemplo: `./pop3d -p 1110 -P 9090 -u userpop3:pass -U userdajt:pass -d ./`. Esta ejecución comenzará el servidor POP3 y creará un usuario de pop3 (userpop3) y uno de dajt (userdajt). Además, establece el directorio de donde se obtienen los emails de los distintos usuarios como `.`.
Los argumentos que permite`./pop3d`

3. Se puede utilizar un MUA para acceder al servidor. Se realizaron pruebas desde la terminal con curl (`curl pop3://userpop3:pass@127.0.0.1:1110/`) y netcat ejecutando `nc -C localhost 1110`.

4. Para acceder al cliente de DAJT (kerl) se debe utilizar el comando `./kerl [-v4|-v6] dajt://admin:admin@127.0.0.1:9090/[:path]`, donde las opciones de path son:
    - `buff/[number]`: permite ver/modificar el buffer de entrada del servidor POP3.
    - `ttra/[0|1|]`: permite ver el estado de las transformaciones, y, habilitarlas y deshabilitarlas
    - `tran [-body "commands"|]`: permite obtener/modificar el comando transformador a ejecutar 
    - `stat/[b|c|t]`: permite ver estadísticas ('b' = total de bytes enviados, 'c' = cantidad de conexiones actuales, 't' = cantidad de conexiones totales)
    - Para más información, consultar `./kerl --help`