#include <Arduino.h>

/**************************************/
/* Variables generales                */
/**************************************/

#define UP 3
#define DOWN 1
#define LEFT 2
#define RIGHT 0
#define GOVER 4

#define MAX 9
#define MIN 0

// Booleano que señaliza que hay que avanzar la lógica de juego
volatile bool timer_flag = false;
// Coordenadas de la serpiente (por defecto al centro de la ventana)
uint8_t snakeX = 4;
uint8_t snakeY = 4;
// Coordenadas de la comida
uint8_t foodX = 0;
uint8_t foodY = 0;
uint8_t foodLastX = 0;
uint8_t foodLastY = 0;

uint8_t state = 255;  // Estado por defecto

uint16_t score = 0;   // Puntuación


ISR(TIMER1_COMPA_vect)
{
  timer_flag = true;
}

void updateFood() {
  foodX = random(7) + 1;
  foodY = random(7) + 1;
  // Si la comida aparece en el mismo sitio de la serpiente o en el mismo sitio, recalcula
  while ((foodX == snakeX && foodY == snakeY) || (foodX == foodLastX && foodY == foodLastY)) {
    foodX = random(7) + 1;
    foodY = random(7) + 1;
  }
}

// Reset del juego: Restablecimiento de parámetros por defecto
void reset() {
  timer_flag = false;
  // Coordenadas de la serpiente (por defecto al centro de la ventana)
  snakeX = 4;
  snakeY = 4;
  // Coordenadas de la comida
  foodX = 0;
  foodY = 0;
  foodLastX = 0;
  foodLastY = 0;
  state = 255;
  score = 0;
  updateFood();
  TCCR1B &= ~(1 << CS11); // Habilita Timer
  OCR1A = 10000;
}


/******************************************************************************/
/** SETUP *********************************************************************/
/******************************************************************************/

void setup()
{
  Serial.begin(115200);

  // Configure timer 1
  // 10 ms; OC = 10; pre-escaler = 1:1024
  TCCR1A = 0;
  TCCR1B = 0;

  TCCR1B |= (1 << WGM12); // CTC => WGMn3:0 = 0100
  OCR1A = 10000;
  TIMSK1 |= (1 << OCIE1A);
  TCCR1B |= (1 << CS10);
  TCCR1B |= (1 << CS12);

  // Seed aleatorio para que la comida no aparezca siempre en los mismos sitios
  randomSeed(analogRead(0));

}


/******************************************************************************/
/** LOOP **********************************************************************/
/******************************************************************************/

void loop()
{
  // TABLA DE ESTADOS
  // 0 = Hacia la derecha
  // 1 = Hacia abajo
  // 2 = Hacia la izquierda
  // 3 = Hacia arriba
  // 4 = Game Over
  // 255 = Default (sin movimiento)
 
  // Tecla recibida
  uint8_t key;

  updateFood();

  while (1)
  {
    // Cuando se verifica una interrupción del Timer
    if (timer_flag)
    {
      // Según hacia dónde va la serpiente, cambia su posición
      switch (state)
      {
        case RIGHT: snakeX += 1; break;
        case DOWN:  snakeY += 1; break;
        case LEFT:  snakeX -= 1; break;
        case UP:    snakeY -= 1; break;
        case GOVER: TCCR1B |= (1 << CS11); break; // Pausa el timer
        default: break;
      }

      // Si la serpiente choca con los límites del campo, game over
      if (snakeX == MAX || snakeY == MAX || snakeX == 0 || snakeY == 0)
        state = 4;

      // Si la serpiente y la comida colisionan, suma 1 a la puntuación,
      // actualiza la posición de la comida y aumenta la velocidad del juego
      if (snakeX == foodX && snakeY == foodY) {
        score++;
        updateFood();

        OCR1A -= 400;
        if (OCR1A < 2000) OCR1A = 2000; // Límite máximo
      }

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
      Serial.println('>');

      timer_flag = false;
    }

    if (Serial.available() > 0)
    {
      key = Serial.read();

      // Control movement
      // UP 38, LEFT 37, RIGHT 39, DOWN 40

      switch (key) {
        // Ni idea de porqué A y S están intercambiados, pero si no no funciona
        case 'A': TCCR1B &= ~(1 << CS11); break; // Reanuda el timer
        case 'S': TCCR1B |= (1 << CS11); break;  // Pausa el timer
        case 'R': reset();

        case 38: state = UP; break;
        case 37: state = LEFT; break;
        case 39: state = RIGHT; break;
        case 40: state = DOWN; break;

        default: state = 255; break;
      }

    }
  }
}