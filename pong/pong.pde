import processing.dxf.*;

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

final int MENU = 1;
final int PLAYING = 2;
final int PAUSE = 3;
final int GAME_OVER = 4;

final int screenSizeX = 1300;
final int screenSizeY = 840;

// Variables globales de estado
volatile int posX1 = 0;
volatile int posY1 = 0;
volatile int posX2 = 0;
volatile int posY2 = 0;

volatile int posBallX = screenSizeX/2;
volatile int posBallY = screenSizeY/2;

volatile int state = 255;
volatile int scoreP1 = 0;
volatile int scoreP2 = 0;

int textMenuSelect = 1;


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

  // Habilita el filtrado trilinear
  smooth(3);
  frameRate(30);

  // Activamos el estado inicial
  state = PLAYING;

  // Inicialización serial (CAMBIAR SEGÚN EL SO!)
  /*
  s = new Serial(this, "COM8", 115200);
   s.bufferUntil('\n'); // Buffer hasta recibir un '\n' antes de llamar al serialEvent
   
   // Delay de 2 seg. para dar tiempo al Arduino de empezar a enviar datos
   delay(2000);
   */
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

  // Modo de pausa
  else if (state == PAUSE)
  {
    fill(0, 0, 0, 100);  // Obscura la pantalla
    rect(0, 0, width, height);
    // Escribe texto de "Pausa"
    textSize(64);
    fill(255, 0, 0);
    textAlign(CENTER, CENTER);
    text("PAUSA", 410, 380);
  }

  // Si estas jugando
  else if (state == PLAYING)
  {
    // Pintar fondo
    image(fondoJuego, 0, 0, screenSizeX, screenSizeY);

    dibujarLineaDivisoria();

    // Ejecutamos estas operaciones solo la primera vez
    if (firstTime)
    {
      //Dibujamos las tablas y la bola
      noStroke();
      fill(255, 255, 255);
      rect(posInicialX1, posInicialY1, inicialWidth, inicialHeight);
      rect(posInicialX2, posInicialY2, inicialWidth, inicialHeight);
      ellipse(posBallX, posBallY, ballRadius, ballRadius);

      //firstTime = false;
    }

    //Aplicamos el shader al fondo
    shader.set("iGlobalTime", millis() / 1000.0);
    filter(shader);

    // Colocamos el frame de la TV encima del fondo
    image(frame, 0 - 100/2, 0 - 70/2 + 5, screenSizeX + 100, screenSizeY + 70);
  } 

  // ...o, si estamos en game over
  else if (state == GAME_OVER) {

    textSize(28);
    text("Press R to restart", 410, 750);
  }
}

void dibujarLineaDivisoria()
{
}

/*
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
 snakeX = Integer.parseInt(tokens[0]);
 snakeY = Integer.parseInt(tokens[1]);
 foodX = Integer.parseInt(tokens[2]);
 foodY = Integer.parseInt(tokens[3]);
 state = Integer.parseInt(tokens[4]);
 score = Integer.parseInt(tokens[5]);
 perepalmer = tokens[6];
 
 // ARDUINO
 
 Serial.print('<');
 Serial.print(snakeX);
 Serial.print(',');
 Serial.print(snakeY);
 Serial.print(',');
 Serial.print(foodX);
 Serial.print(',');
 Serial.print(foodY);
 Serial.print(',');
 Serial.print(state);
 Serial.print(',');
 Serial.print(score);
 Serial.print(',');
 Serial.print(perepalmer);
 Serial.println('>');
 
 // Imprime por consola los datos actualizados
 println("X: " + snakeX + " | Y: " + snakeY + " | FoodX: " + foodX + " | FoodY: " + foodY + " | state: " + state + " | score: " + score);
 } 
 catch (NumberFormatException nfe) {
 // Si se verifica un error durante el parsing de los tokens, vuelve a dibujar la pantalla
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
 */
