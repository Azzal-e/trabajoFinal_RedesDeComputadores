/****************************************************************************/
/* Plantilla para implementación de funciones del cliente (rcftpclient)     */
/* $Revision$ */
/* Aunque se permite la modificación de cualquier parte del código, se */
/* recomienda modificar solamente este fichero y su fichero de cabeceras asociado. */
/****************************************************************************/

/**************************************************************************/
/* INCLUDES                                                               */
/**************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "rcftp.h" // Protocolo RCFTP
#include "rcftpclient.h" // Funciones ya implementadas
#include "multialarm.h" // Gestión de timeouts
#include "vemision.h" // Gestión de ventana de emisión
#include "misfunciones.h"

/**************************************************************************/
/* VARIABLES GLOBALES                                                     */
/**************************************************************************/
/**
 * Longitud de buffer de datos
 */
#define RCFTP_BUFLEN 512
// elegir 1 o 2 autores y sustituir "Apellidos, Nombre" manteniendo el formato
//char* autores="Autor: Usero Samaras, Athanasios"; // un solo autor
char* autores="Autor: Usero Samarás, Athanasios\nAutor: Sanchez Sarsa, Eduardo"; // dos autores

// variable para indicar si mostrar información extra durante la ejecución
// como la mayoría de las funciones necesitaran consultarla, la definimos global
extern char verb;

/* defines para colorear salida */
/** @{ */
#define ANSI_COLOR_RED     "\x1b[31m" /**< Pone terminal a rojo */
#define ANSI_COLOR_GREEN   "\x1b[32m" /**< Pone terminal a verde */
#define ANSI_COLOR_YELLOW  "\x1b[33m" /**< Pone terminal a amarillo */
#define ANSI_COLOR_BLUE    "\x1b[34m" /**< Pone terminal a azul */
#define ANSI_COLOR_MAGENTA "\x1b[35m" /**< Pone terminal a magenta */
#define ANSI_COLOR_CYAN    "\x1b[36m" /**< Pone terminal a turquesa */
#define ANSI_COLOR_RESET   "\x1b[0m" /**< Desactiva color del terminal */
/** @} */

/* defines para la salida del programa */
/** @{ */
#define S_OK 0 /**< Flag de salida correcta */
#define S_ABORT 1 /**< Flag de salida incorrecta */
#define S_SYSERROR 2 /**< Flag de salida por error de sistema */
#define S_PROGERROR 3 /**< Flag de salida por error de programa (BUG!) */
#define S_CLIERROR 4 /**< Flag de salida para avisar de error en cliente */
/** @} */

#define MAXVEMISION 10240

//#define SIG_ERR (void (*)()) -1 // Error en el manejo de señales


// variable externa que muestra el número de timeouts vencidos
// Uso: Comparar con otra variable inicializada a 0; si son distintas, tratar un timeout e incrementar en uno la otra variable
extern volatile const int timeouts_vencidos;


/**************************************************************************/
/* Obtiene la estructura de direcciones del servidor */
/**************************************************************************/
struct addrinfo* obtener_struct_direccion(char *dir_servidor, char *servicio, char f_verbose){
    struct addrinfo hints,     // Variable para especificar la solicitud
                    *servinfo, // Puntero para respuesta de getaddrinfo()
                    *direccion;// Puntero para recorrer la lista de
                               // direcciones de servinfo
    int status;     // Finalización correcta o no de la llamada getaddrinfo()
    int numdir = 1; // Contador de estructuras de direcciones en la
                    // lista de direcciones de servinfo

    // sobreescribimos con ceros la estructura
    // para borrar cualquier dato que pueda malinterpretarse
    memset(&hints, 0, sizeof hints);

    // genera una estructura de dirección con especificaciones de la solicitud
    if (f_verbose)
    {
        printf("1 - Especificando detalles de la estructura de direcciones a solicitar... \n");
        fflush(stdout);
    }

    // especificamos la familia de direcciones con la que queremos trabajar:
    // AF_UNSPEC, AF_INET (IPv4), AF_INET6 (IPv6), etc.
    hints.ai_family = AF_UNSPEC ;

    if (f_verbose)
    {
        printf("\tFamilia de direcciones/protocolos: ");
        switch (hints.ai_family)
        {
            case AF_UNSPEC: printf("IPv4 e IPv6\n"); break;
            case AF_INET:   printf("IPv4\n"); break;
            case AF_INET6:  printf("IPv6\n"); break;
            default:        printf("No IP (%d)\n", hints.ai_family); break;
        }
        fflush(stdout);
    }

    // especificamos el tipo de socket deseado:
    // SOCK_STREAM (TCP), SOCK_DGRAM (UDP), etc.
    hints.ai_socktype = SOCK_DGRAM;

    if (f_verbose)
    {
        printf("\tTipo de comunicación: ");
        switch (hints.ai_socktype)
        {
            case SOCK_STREAM: printf("flujo (TCP)\n"); break;
            case SOCK_DGRAM:  printf("datagrama (UDP)\n"); break;
            default:          printf("no convencional (%d)\n", hints.ai_socktype); break;
        }
        fflush(stdout);
    }

    // flags específicos dependiendo de si queremos la dirección como cliente
    // o como servidor
    if (dir_servidor != NULL)
    {
        // si hemos especificado dir_servidor, es que somos el cliente
        // y vamos a conectarnos con dir_servidor
        if (f_verbose) printf("\tNombre/dirección del equipo: %s\n", dir_servidor);
    }
    else
    {
        // si no hemos especificado, es que vamos a ser el servidor
        if (f_verbose) printf("\tNombre/dirección: equipo local\n");

        // especificar flag para que la IP se rellene con lo necesario para hacer bind
        // consultar documentación con: 'man getaddrinfo')
        hints.ai_flags = AI_PASSIVE ;
    }
    if (f_verbose) printf("\tServicio/puerto: %s\n", servicio);

    // llamada getaddrinfo() para obtener la estructura de direcciones solicitada
    // getaddrinfo() pide memoria dinámica al SO,
    // la rellena con la estructura de direcciones
    // y escribe en servinfo la dirección donde se encuentra dicha estructura.
    // La memoria dinámica reservada en una función NO se libera al salir de ella
    // Para liberar esta memoria, usar freeaddrinfo()
    if (f_verbose)
    {
        printf("2 - Solicitando la estructura de direcciones con getaddrinfo()... ");
        fflush(stdout);
    }
    status = getaddrinfo(dir_servidor, servicio, &hints, &servinfo);
    if (status != 0)
    {
        fprintf(stderr,"Error en la llamada getaddrinfo(): %s\n", gai_strerror(status));
        exit(1);
    }
    if (f_verbose) printf("hecho\n");

    // imprime la estructura de direcciones devuelta por getaddrinfo()
    if (f_verbose)
    {
        printf("3 - Analizando estructura de direcciones devuelta... \n");
        direccion = servinfo;
        while (direccion != NULL)
        {   // bucle que recorre la lista de direcciones
            printf("    Dirección %d:\n", numdir);
            printsockaddr((struct sockaddr_storage*) direccion->ai_addr);
            // "avanzamos" a la siguiente estructura de direccion
            direccion = direccion->ai_next;
            numdir++;
        }
    }

    // devuelve la estructura de direcciones devuelta por getaddrinfo()
    return servinfo;
}
/**************************************************************************/
/* Imprime una direccion */
/**************************************************************************/
void printsockaddr(struct sockaddr_storage * saddr) {
    struct sockaddr_in *saddr_ipv4; // puntero a estructura de dirección IPv4
    // el compilador interpretará lo apuntado como estructura de dirección IPv4
    struct sockaddr_in6 *saddr_ipv6; // puntero a estructura de dirección IPv6
    // el compilador interpretará lo apuntado como estructura de dirección IPv6
    void *addr; // puntero a dirección
    // como puede ser tipo IPv4 o IPv6 no queremos que el compilador la
    // interprete de alguna forma particular, por eso void
    char ipstr[INET6_ADDRSTRLEN]; // string para la dirección en formato texto
    int port; // almacena el número de puerto al analizar estructura devuelta

    if (saddr == NULL)
    {
        printf("La dirección está vacía\n");
    }
    else
    {
        printf("\tFamilia de direcciones: ");
        fflush(stdout);
        if (saddr->ss_family == AF_INET6)
        {   // IPv6
            printf("IPv6\n");
            // apuntamos a la estructura con saddr_ipv6 (cast evita warning),
            // así podemos acceder al resto de campos a través de
            // este puntero sin más casts
            saddr_ipv6 = (struct sockaddr_in6 *)saddr;
            // apuntamos al campo de la estructura que contiene la dirección
            addr = &(saddr_ipv6->sin6_addr);
            // obtenemos el puerto, pasando del formato de red al formato local
            port = ntohs(saddr_ipv6->sin6_port);
        }
        else if (saddr->ss_family == AF_INET)
        {   // IPv4
            printf("IPv4\n");
            // apuntamos a la estructura con saddr_ipv4 (cast evita warning),
            // así podemos acceder al resto de campos a través de este puntero
            // sin más casts
            saddr_ipv4 = (struct sockaddr_in *)saddr;
            // apuntamos al campo de la estructura que contiene la dirección
            addr = &(saddr_ipv4->sin_addr);
            // obtenemos el puerto, pasando del formato de red al formato local
            port = ntohs(saddr_ipv4->sin_port);
        }
        else
        {
            fprintf(stderr, "familia desconocida\n");
            exit(1);
        }
        // convierte la dirección ip a string
        inet_ntop(saddr->ss_family, addr, ipstr, sizeof ipstr);
        printf("\tDirección (interpretada según familia): %s\n", ipstr);
        printf("\tPuerto (formato local): %d\n", port);
    }
}
/**************************************************************************/
/* Configura el socket, devuelve el socket y servinfo */
/**************************************************************************/
int initsocket(struct addrinfo *servinfo, char f_verbose){
    int sock = -1;

    // crea un extremo de la comunicación y devuelve un descriptor
    if (f_verbose)
    {
            printf("Creando el socket (socket)... ");
            fflush(stdout);
    }
    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock < 0)
    {
          perror("Error en la llamada socket: No se pudo crear el socket");
          // muestra por pantalla el valor de la cadena suministrada por el
          // programador, dos puntos y un mensaje de error que detalla la
          // causa del error cometido
          exit(1);
    }
    else
    {   // socket creado correctamente
    if (f_verbose) printf("hecho\n");
    }

        // Ya no se avanza en una estrucutra de direcciones.
        //servinfo = servinfo->ai_next;
        // numdir++;
    return sock;
}

/**************************************************************************/
/*  Construye un mensaje válido según el protocolo RCFTP  */
/**************************************************************************/

void construirMensajeRCFTP(struct rcftp_msg *mensaje,int antLen){
  mensaje->version = RCFTP_VERSION_1; // EDU
  mensaje->flags = F_NOFLAGS; // Aquí me resguardo ante cualquier fallo.
  if(ntohs(mensaje->len) == 0){
    mensaje->flags = F_FIN;
  }
   mensaje->numseq = htonl(ntohl(mensaje->numseq) + antLen);
   mensaje->next  = htonl(0);
   mensaje->sum = 0;
   mensaje->sum = xsum((char *) mensaje, sizeof(*mensaje));
   	  // print response if in verbose mode
	  if (verb) {
		  printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "construido" ANSI_COLOR_RESET ":\n");
		  print_rcftp_msg(mensaje,sizeof(*mensaje));
	  } 
}

// EDU
void construirMensajeAux(struct rcftp_msg *mensajeViejo, int len, int ultimoMensajeViejo, int ultimoMensaje){
  mensajeViejo->version = RCFTP_VERSION_1;
  if ( ultimoMensaje == 1 && ultimoMensajeViejo == 1){
    mensajeViejo->flags = F_FIN;
  } else{
    mensajeViejo->flags = F_NOFLAGS;
  }
  mensajeViejo->len = htons(len);
  mensajeViejo->next  = htonl(0);
  mensajeViejo->sum = 0;
  mensajeViejo->sum = xsum((char *)mensajeViejo, sizeof(*mensajeViejo));
}


/**************************************************************************/
/*  Enviar Mensaje  */
/**************************************************************************/

void enviarMensajeRCFTP(int socket, struct sockaddr *addr, socklen_t addrlen, struct rcftp_msg *mensaje){
    ssize_t mensajeLen;
    if((mensajeLen = sendto(socket, (char *)mensaje,sizeof(*mensaje),0, addr, addrlen)) !=
        sizeof(*mensaje)) {
         	if (mensajeLen!=-1){
			        fprintf(stderr,"Error: enviados %d bytes de un mensaje de %d bytes\n",(int)mensajeLen,
                (int)sizeof(*mensaje));
          }
		      else{
			      perror("Error en sendto");
           exit(S_SYSERROR);
          }
	  } 
	  // print response if in verbose mode
	  if (verb) {
		  printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "enviado" ANSI_COLOR_RESET ":\n");
		  print_rcftp_msg(mensaje,sizeof(*mensaje));
	  } 
}

/**************************************************************************/
/*  RecibirMensaje  */
/**************************************************************************/

ssize_t recibirMensajeRCFTP(int socket, struct rcftp_msg *mensaje, int mensajelen, struct sockaddr addr,
                              socklen_t addrlen){

  ssize_t recvtam;
  addrlen = sizeof(addr);
	recvtam = recvfrom(socket,(char *)mensaje,mensajelen,0,(struct sockaddr *)&addr,&addrlen);
	if (recvtam<0 && errno!=EAGAIN) { // en caso de socket no bloqueante
		perror("Error en recvfrom: ");
		exit(S_SYSERROR);
	} else if (addrlen>sizeof(addr)) {
		fprintf(stderr,"Error: la dirección del cliente ha sido truncada\n");
		exit(S_SYSERROR);
	}
   return recvtam;
}

/**************************************************************************/
/* Verifica version, checksum */
/**************************************************************************/
int esMensajeValido(struct rcftp_msg mensaje) { 
	int esperado=1;

	if (mensaje.version!=RCFTP_VERSION_1) { // versión incorrecta
		esperado = 0;
		fprintf(stderr,"Error: recibido un mensaje con versión incorrecta\n");
	}
	if (issumvalid(&mensaje,sizeof(mensaje))==0) { // checksum incorrecto
		esperado=0;
		fprintf(stderr,"Error: recibido un mensaje con checksum incorrecto\n");
	}
	return esperado;
}

/**************************************************************************/
/* Verifica next y flags */
/**************************************************************************/
int esLaRespuestaEsperada(struct rcftp_msg respuesta, struct rcftp_msg mensaje){
  int esperado = 1;

  if(ntohl(respuesta.next) != ntohl(mensaje.numseq) + ntohs(mensaje.len)){
		esperado=0;
		fprintf(stderr,"Error: recibido un mensaje next inválido\n");
  }
  if(((respuesta.flags & F_ABORT) == F_ABORT) || ((respuesta.flags & F_BUSY) == F_BUSY)){

    esperado=0;
		fprintf(stderr,"Error: respuesta con 'ocupado/abortar' \n");
  }
  if(((mensaje.flags & F_FIN) == F_FIN) && ((respuesta.flags & F_FIN) != F_FIN)){
  
    esperado=0;
		fprintf(stderr,"Error: Desincronización de fin \n");
  }
  
  return esperado;
  
}

/**************************************************************************/
/*  algoritmo 1 (basico)  */
/**************************************************************************/
void alg_basico(int socket, struct addrinfo *servinfo) {

	printf("Comunicación con algoritmo básico\n");
 
   struct rcftp_msg mensaje, respuesta;
   int ultimoMensaje = 0;
   int ultimoMensajeConfirmado = 0;
   int antLen = 0;
   
   int l = readtobuffer((char *) mensaje.buffer, RCFTP_BUFLEN); // Leer de la entrada estandar
   if(l == 0){ // No se ha leído nada (fin del fichero)
     ultimoMensaje = 1;
   }
  
  
  mensaje.numseq = ntohl(0); // Inicializar campo de mensaje invariable.
  construirMensajeAux(&mensaje, l, 1, ultimoMensaje); 
   
   // Bucle para enviar mensaje y procesar respuesta
   while(ultimoMensajeConfirmado == 0){
     
     // Enviamos mensaje y esperamos a respuesta del servidor
     enviarMensajeRCFTP(socket,servinfo->ai_addr, servinfo->ai_addrlen, &mensaje); 
     recibirMensajeRCFTP(socket, &respuesta, sizeof(respuesta),*servinfo->ai_addr, servinfo->ai_addrlen);
     if( esMensajeValido(respuesta) == 1 && esLaRespuestaEsperada(respuesta, mensaje) == 1){ // Si mensaje válido
       
       if(ultimoMensaje == 1){
         ultimoMensajeConfirmado = 1;
       }
       else{
         antLen = l;
         l = readtobuffer((char *) mensaje.buffer, RCFTP_BUFLEN); // Leer de la entrada estandar
         if(l == 0){ // No se ha leído nada (fin del fichero)
           ultimoMensaje = 1;
         }
         // CONSTRUIR MENSAJE
         mensaje.len = htons(l); // Actualizamos longitud del mensaje
         construirMensajeRCFTP(&mensaje, antLen); //antLen (longitud del anterior mens.) para construir nuevo numSeq
       }
     }
   } 
}

/**************************************************************************/
/*  Recibir mensaje en STOP AND WAIT  */
/**************************************************************************/
int recibirMensajeStopWait(int socket, struct rcftp_msg  *respuesta,int *timeouts_procesados,
                             struct sockaddr addr, socklen_t addrlen){
    
    ssize_t numDatosRecibidos;
    int recibido =0;
    
    addtimeout(); // Programamos timeout
    int esperar = 1;
    while(esperar == 1){ // Mientras haya que esperar
      numDatosRecibidos = recibirMensajeRCFTP(socket, respuesta, sizeof(*respuesta),
                                               addr, addrlen);
      // Comprobar si se ha recibido el memnsaje
      if(numDatosRecibidos > 0){
        canceltimeout(); // Se puede realizar también después de comprobar mensaje en sí
        esperar = 0;
        recibido = 1;
        
        if (verb) {
		      printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "detectado" ANSI_COLOR_RESET "\n");
	        } 
      }
      
      // Comprobar si ha vencido el timeout 
      if (*timeouts_procesados != timeouts_vencidos){
        esperar = 0;
        *timeouts_procesados = (*timeouts_procesados) + 1;
        
          if (verb) { 
		        printf("Timeout " ANSI_COLOR_MAGENTA "vencido" ANSI_COLOR_RESET "\n");
	        } 
      }
    }
    
    return recibido;
}

/**************************************************************************/
/*  algoritmo 2 (stop & wait)  */
/**************************************************************************/
void alg_stopwait(int socket, struct addrinfo *servinfo) {

	printf("Comunicación con algoritmo stop&wait\n");
 
   // AJUSTES PREVIOS
   struct rcftp_msg mensaje, respuesta;
   int ultimoMensaje = 0;
   int ultimoMensajeConfirmado = 0;
   int antLen = 0;
   int timeouts_procesados = 0;
   
   // Asociar rutina de servicio de la alarma a la función handle_sigalrm
   if(signal(SIGALRM, handle_sigalrm) == SIG_ERR){
      printf("Error al especificar el comportamiento de la señal");
   }
   
   // Establecer socket <<no bloqueante>> para manejar interrupciones correctamente 
   int sockflags;
   sockflags = fcntl (socket, F_GETFL, 0); // obtiene el valor de los flags
   fcntl(socket, F_SETFL, sockflags | O_NONBLOCK);
 
   
   int l = readtobuffer((char *) mensaje.buffer, RCFTP_BUFLEN); // Leer de la entrada estandar
   if(l == 0){ // No se ha leído nada (fin del fichero)
     ultimoMensaje = 1;
   }
   
   // Inicializar mensaje
  mensaje.numseq = ntohl(0);
  construirMensajeAux(&mensaje, l, 1 ,ultimoMensaje); // EDU

   
   // Bucle para enviar mensaje y procesar respuesta
   while(ultimoMensajeConfirmado == 0){
     
     enviarMensajeRCFTP(socket,servinfo->ai_addr, servinfo->ai_addrlen, &mensaje); // Enviamos mensaje
     
     if(recibirMensajeStopWait(socket, &respuesta, &timeouts_procesados,
                             *servinfo->ai_addr, servinfo->ai_addrlen) == 1){ // Si se ha recibido respuesta
       if (verb) {
		      printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "recibido" ANSI_COLOR_RESET ":\n");
		      print_rcftp_msg(&respuesta,sizeof(respuesta));
       }
                             
       if( esMensajeValido(respuesta) == 1 && esLaRespuestaEsperada(respuesta, mensaje) == 1){ // Mensaje válido
         if(ultimoMensaje == 1){
           ultimoMensajeConfirmado = 1;
         }
         else{
           antLen = l;
           l = readtobuffer((char *) mensaje.buffer, RCFTP_BUFLEN); // Leer de la entrada estandar
           if(l == 0){ // No se ha leído nada (fin del fichero)
             ultimoMensaje = 1;
           }
           // CONSTRUIR MENSAJE
           mensaje.len = htons(l);
           construirMensajeRCFTP(&mensaje, antLen); 
         }
       }
     }
   } 
}

/**************************************************************************/
/*  next-1 dentro de la ventana de emisión y que no haya flags "malos"  */
/**************************************************************************/

int esLaRespuestaEsperadaGBN(struct rcftp_msg respuesta, struct rcftp_msg mensaje, int window){
  int esperado = 1;
  char bufferTemp[MAXVEMISION];
  int len;
  
  // Obtención de extremos de la ventana de emisión
  len = 0;
  uint32_t numSeqInit = getdatatoresend(bufferTemp, &len); // Obtenemos primer número de secuencia
  
  len = window - getfreespace(); // Calculamos el número de bytes almacenados en el presente momento en la ve
  uint32_t numSeqFin = numSeqInit + len; // Cálculo del último número de secuencia almacenado
  //printf("[%d , %d]\n", numSeqInit, numSeqFin); 
  //printvemision();
  
  if(ntohl(respuesta.next) - 1 > numSeqFin || ntohl(respuesta.next) - 1 < numSeqInit){
    // next fuera de la ventana de emisión
		esperado = 0;
		fprintf(stderr,"Error: recibido un mensaje next fuera de la ventana de emisión\n");
    
  }
  
  if(((respuesta.flags & F_ABORT) == F_ABORT) || ((respuesta.flags & F_BUSY) == F_BUSY)){ // Flags incompatibles
    
    esperado = 0;
		fprintf(stderr,"Error: respuesta con 'ocupado/abortar' \n");
  }
  
  return esperado;
  
}

/**************************************************************************/
/*  construirMensaje v.2 (ventana deslizante)  */
/* NOTA: La diferencia es que tiene en cuenta si es un último mensaje ya que
// el envío de mensaje ya no sigue un orden estrictamente secuencial, sino
// habrá momentos en los que se reconstruya desde atrás*/
/**************************************************************************/

void construirMensajeRCFTP2(struct rcftp_msg *mensaje,int antLen, int ultimoMensaje){
  mensaje->version = RCFTP_VERSION_1;
  mensaje->flags = F_NOFLAGS; // Aquí me resguardo ante cualquier fallo.
  if(ultimoMensaje == 1){
    mensaje->flags = F_FIN;
  }
   mensaje->numseq = htonl(ntohl(mensaje->numseq) + antLen);
   mensaje->next  = htonl(0);
   mensaje->sum = 0;
   mensaje->sum = xsum((char *) mensaje, sizeof(*mensaje));
   	// Sacamos por pantalla si está en modo verbose.
	  if (verb) {
		  printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "construido" ANSI_COLOR_RESET ":\n");
		  print_rcftp_msg(mensaje,sizeof(*mensaje));
	  } 
}



/**************************************************************************/
/*  algoritmo 3 (ventana deslizante)  */
/**************************************************************************/
void alg_ventana(int socket, struct addrinfo *servinfo,int window) {

	printf("Comunicación con algoritmo go-back-n\n");
  
  
     // AJUSTES PREVIOS
   
   // Asociar rutina de servicio de la alarma a la función handle_sigalrm
   if(signal(SIGALRM, handle_sigalrm) == SIG_ERR){
      printf("Error al especificar el comportamiento de la señal");
   }
   
   // Establecer socket <<no bloqueante>> para manejar interrupciones correctamente 
   int sockflags;
   sockflags = fcntl (socket, F_GETFL, 0); // obtiene el valor de los flags
   fcntl(socket, F_SETFL, sockflags | O_NONBLOCK);
 
   setwindowsize(window);

   int ultimoMensaje = 0, ultimoMensajeViejo = 0;
   int ultimoMensajeConfirmado = 0;
   int antLen = 0;
   struct rcftp_msg mensaje, respuesta, mensajeViejo;
   // int esperar; // Para esperar o no al envío de una llamada
   int timeouts_procesados = 0;
   int espacioLibreVE, espacioOcupadoVE;
   ssize_t numDatosRecibidos;
   int l;
   
   //int l = readtobuffer((char *) mensaje.buffer, RCFTP_BUFLEN); // Leer de la entrada estandar
   //if(l == 0){ // No se ha leído nada (fin del fichero)
     //ultimoMensaje = 1;
   //}
   
   // Inicializar mensaje
   
   mensaje.version = RCFTP_VERSION_1; // Version no cambia 
   mensaje.numseq = htonl(0);
   
   mensajeViejo.version = RCFTP_VERSION_1; // Version no cambia 
   mensajeViejo.numseq = htonl(0);

  while (ultimoMensajeConfirmado == 0){
    /*** BLOQUE DE ENVIO: Enviaar datos si hay espacio en la ventana***/
    espacioLibreVE = getfreespace();
    if(espacioLibreVE >  RCFTP_BUFLEN){ // Reducimos tamaño al máximo del bufer en caso de que proceda
      espacioLibreVE = RCFTP_BUFLEN;
    }
    if(espacioLibreVE > 0 && ultimoMensaje == 0){
      antLen = l;
      l = readtobuffer((char *) mensaje.buffer, espacioLibreVE);
      
      if(l < espacioLibreVE){ // Se ha leído menos === FIN DE FICHERO
        ultimoMensaje =  1;    
      }
      mensaje.len = htons(l);
      construirMensajeRCFTP2(&mensaje, antLen, ultimoMensaje);
      enviarMensajeRCFTP(socket,servinfo->ai_addr, servinfo->ai_addrlen, &mensaje); 
      addtimeout();
      if(addsentdatatowindow((char *) mensaje.buffer, l) == l){ // Se han añadido todos los datos
        if(verb){
          printf("Datos Añadidos a ventana de emisión: \n");
        }
      }
      else{
        if(verb){
          printf("Algo raro con los datos en VE \n");
        }
      }
      //printvemision();
    }
    
    /***BLOQUE DE RECEPCIÓN: Recibir respuesta y procesarla (si existe)***/
    numDatosRecibidos = recibirMensajeRCFTP(socket, &respuesta, sizeof(respuesta),
                                               *servinfo->ai_addr, servinfo->ai_addrlen);
    if(numDatosRecibidos > 0){
       if (verb) {
		      printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "recibido" ANSI_COLOR_RESET ":\n");
		      print_rcftp_msg(&respuesta,sizeof(respuesta));
       }
      if (esMensajeValido(respuesta) == 1 && esLaRespuestaEsperadaGBN(respuesta, mensaje, window) == 1){
        canceltimeout();
        freewindow(ntohl(respuesta.next));
        if((respuesta.flags & F_FIN) == F_FIN){
            ultimoMensajeConfirmado = 1;
        }
      }
    }
    /***BLOQUE DE PROCESADO DE TIMEOUT***/
    if(timeouts_procesados != timeouts_vencidos){
      //mensaje <- construirmensajeMasViejoDeVentanaDeEmision()
      espacioOcupadoVE = window - getfreespace();
      if(espacioOcupadoVE > RCFTP_BUFLEN){ 
        espacioOcupadoVE = RCFTP_BUFLEN;
        ultimoMensajeViejo = 0;
      }
      else{
        ultimoMensajeViejo = 1;
      }
      mensajeViejo.numseq = htonl(getdatatoresend((char *) mensajeViejo.buffer, &espacioOcupadoVE));
      if( ultimoMensaje == 1 && ultimoMensajeViejo == 1){
        mensajeViejo.flags = F_FIN;
      }
      else{
        mensajeViejo.flags = F_NOFLAGS;
      }
      mensajeViejo.len = htons(espacioOcupadoVE);
      mensajeViejo.next  = htonl(0);
      mensajeViejo.sum = 0;
      mensajeViejo.sum = xsum((char *) &mensajeViejo, sizeof(mensajeViejo));
   	  // print response if in verbose mode
	    if (verb) {
		    printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "reconstruido" ANSI_COLOR_RESET ":\n");
		    print_rcftp_msg(&mensajeViejo,sizeof(mensaje));
	    } 
      //********CONSTRUIR MENSAJE************
      enviarMensajeRCFTP(socket,servinfo->ai_addr, servinfo->ai_addrlen, &mensajeViejo); 
      addtimeout();
      timeouts_procesados = timeouts_procesados + 1;
    }
  
  }
}


