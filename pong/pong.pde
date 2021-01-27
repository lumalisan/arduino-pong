import processing.dxf.*;
import ddf.minim.signals.*;
import ddf.minim.*;
import ddf.minim.analysis.*;
import ddf.minim.ugens.*;
import ddf.minim.effects.*;

// Comunicación serial
import processing.serial.*;

// Variables globales
Serial s;
PShader shader;
PImage fondoMenu, fondoJuego, frame;
int textoMenu = 64;
boolean firstTime = true;

// Constantes
final int inicialWidth = 30;
final int inicialHeight = 100;
final int ballRadius = 30;

final int posInicialX1 = 50;
final int posInicialY1 = 420 - inicialHeight/2;
final int posInicialX2 = 1290 - inicialWidth/2 - posInicialX1;
final int posInicialY2 = 420 - inicialHeight/2;

final int MENU = 0;
final int PLAYING = 1;
final int PAUSED = 2;

final int screenSizeX = 1300;
final int screenSizeY = 840;

// Variables globales de estado
volatile int posX1 = posInicialX1;
volatile int posY1 = posInicialY1;
volatile int posX2 = posInicialX2;
volatile int posY2 = posInicialY2;

volatile int posBallX = screenSizeX/2;
volatile int posBallY = screenSizeY/2;

volatile int state = 255;
volatile int scoreP1 = 0;
volatile int scoreP2 = 0;

int textMenuSelect = 1;

int debug = 0;

// Variables para los threads de musica
MyThread sonar_menu, sonar_play;

boolean once = true;

void setup()
{
  // Crea la ventana
  size(1300, 840, P2D);

  // Cargamos el shader 
  shader = loadShader("vhs.glsl"); 
  shader.set("iResolution", float(width), float(height));

  // Cargamos las imagenes
  fondoMenu = loadImage("fondoMenu.png");
  fondoJuego = loadImage("retrobg2.jpg");
  frame = loadImage("oldTvFrame.png");

  // Cargamos la música
  Minim minim_menu = new Minim(this);
  AudioPlayer player_menu = minim_menu.loadFile("MUSIC2.wav");
  player_menu.setGain(-30);

  // Creamos el thread y lo empezamos
  sonar_menu = new MyThread(minim_menu, player_menu);
  sonar_menu.start();

  Minim minim_play = new Minim(this);
  AudioPlayer player_play = minim_play.loadFile("MUSIC1.wav");
  player_play.setGain(-30);

  // Creamos el thread y lo empezamos
  sonar_play = new MyThread(minim_play, player_play);
  sonar_play.start();

  // Activamos la música del menú
  sonar_menu.playNow();

  // Habilita el filtrado trilinear
  smooth(3);
  frameRate(60);

  // Activamos el estado inicial
  state = MENU;

  // Inicialización serial (CAMBIAR SEGÚN EL SO!)

  s = new Serial(this, "COM6", 115200);
  s.bufferUntil('\n'); // Buffer hasta recibir un '\n' antes de llamar al serialEvent

  // Delay de 2 seg. para dar tiempo al Arduino de empezar a enviar datos
  // delay(2000);
}

void draw()
{
  // Si estas en el menú
  if (state == MENU)
  {
    loop();
    
    // Pintar fondo
    image(fondoMenu, 0, 0, screenSizeX, screenSizeY);

    //Aplicamos el shader
    shader.set("iGlobalTime", millis() / 1000.0);
    filter(shader);

    // Escribir título
    if (textoMenu < 85 && textoMenu >= 64) {
      textoMenu += textMenuSelect;
    } else if (textoMenu >= 85) {
      textMenuSelect = -textMenuSelect;
      textoMenu = 84;
    } else if (textoMenu < 64) {
      textMenuSelect = -textMenuSelect;
      textoMenu = 64;
    }

    textSize(textoMenu);
    fill(255, 255, 255);
    textAlign(CENTER, CENTER);
    text("PONG GAME", 650, 200);

    // Escribir explicaciones
    textSize(20);
    noStroke();
    fill(0, 0, 0, 150);
    rect(365, 485, 565, 60);
    fill(255, 255, 255);
    textAlign(CENTER, CENTER);
    text("Bienvenido al famoso juego del pong, para jugar debes", 650, 500);
    text("utilizar el joystick en las direcciones arriba y abajo.", 650, 520);

    fill(0, 0, 0, 150);
    rect(417, 585, 460, 60);
    fill(255, 255, 255);
    text("Para empeza el juego, tanto el J1 como el J2", 650, 600);
    text("debéis pulsar el joystick al mismo tiempo.", 650, 620);
  }
  /*
  // Modo de pausa
   else if (state == PAUSED)
   {
   fill(0, 0, 0, 20);  // Obscura la pantalla
   rect(0, 0, width, height);
   // Escribe texto de "Pausa"
   textSize(64);
   fill(255, 0, 0);
   textAlign(CENTER, CENTER);
   text("PAUSA", 410, 380);
   noLoop();
   }
   */

  // Si estas jugando
  else if (state == PLAYING)
  {
    if(once){
      sonar_menu.quit();
      sonar_play.playNow();
      once = false;
    }
    noLoop();

    // Pintar fondo
    image(fondoJuego, 0, 0, screenSizeX, screenSizeY);

    dibujarLineaDivisoria();
    mostrarPuntuacion();

    // Ejecutamos estas operaciones solo la primera vez
    //if (firstTime)
    //{
    //Dibujamos las tablas y la bola
    noStroke();
    fill(255, 255, 255);
    rect(posX1, posY1, inicialWidth, inicialHeight);
    rect(posX2, posY2, inicialWidth, inicialHeight);
    ellipse(posBallX, posBallY, ballRadius, ballRadius);

    //firstTime = false;
    //}

    //Aplicamos el shader al fondo
    shader.set("iGlobalTime", millis() / 1000.0);
    filter(shader);

    // Colocamos el frame de la TV encima del fondo
    image(frame, 0 - 100/2, 0 - 70/2 + 5, screenSizeX + 100, screenSizeY + 70);
  }
}

void dibujarLineaDivisoria()
{
  noStroke();
  fill(255, 255, 255, 100);

  for (int i = 0; i < 11; i++) {
    rect(645, i*79, 10, 69);
  }
}

void mostrarPuntuacion()
{
  //debug++;

  textSize(45);
  fill(255, 255, 255);
  textAlign(RIGHT, CENTER);
  text(scoreP1, 605, 50);
  textAlign(LEFT, CENTER);
  text(scoreP2, 690, 50);
}


void serialEvent(Serial s)
{ 
  // El try-catch evita de que se interrumpa la comunicación si el parsing
  // de la información recibida no va bien

  if (state != 4) {
    try {
      s.bufferUntil('\n');  // Almacena en el buffer hasta final de linea
      String input = s.readString();
      println("DEBUG Input: " + input.trim());
      // Elimina los marcadores de inicio/fin y el carácter de nueva línea al final
      input = input.replaceAll("<", "");
      input = input.replaceAll(">", "");
      input = input.trim();

      // Divide en tokens separados por la coma
      String[] tokens = splitTokens(input, ",");

      // DEBUG - Impresión de los tokens por consola
      // for (int i=0; i<tokens.length; i++) {
      //  println("\tDEBUG Token " + i + ": " + tokens[i]);
      // }

      // Actualiza las variables globales según los datos recibidos
      posY1 = Integer.parseInt(tokens[0]);
      posY2 = Integer.parseInt(tokens[1]);
      state = Integer.parseInt(tokens[2]);
      scoreP1 = Integer.parseInt(tokens[3]);
      scoreP2 = Integer.parseInt(tokens[4]);
      posBallX = (int) Float.parseFloat(tokens[5]);
      posBallY = (int) Float.parseFloat(tokens[6]);

      // Imprime por consola los datos actualizados
      println("Y1: " + posY1 + " | Y2: " + posY2 + " | state: " + state + " | scoreP1: " + scoreP1 + " | scoreP2: " + scoreP2 + "\nposBallX: " + posBallX + " | posBallY: " + posBallY);
    } 
    catch (NumberFormatException nfe) {
      // Si se verifica un error durante el parsing de los tokens, vuelve a dibujar la pantalla
      println("NUMBERFORMATEXCEPTION, PUTOS FLOATS DE MIERDA");
    }
    catch (Exception e) {
      // Si hay algún otro tipo de excepción, imprime los detalles y vuelve a dibujar la pantalla
      println("ERROR - " + e.getMessage());
    } 
    finally {
      redraw();  // Vuelve a dibujar la pantalla
    }
  }
} 
