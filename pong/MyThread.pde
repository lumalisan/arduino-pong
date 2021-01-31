// Libreria para la música
import ddf.minim.spi.*;
import ddf.minim.signals.*;
import ddf.minim.*;
import ddf.minim.analysis.*;
import ddf.minim.ugens.*;
import ddf.minim.effects.*;

// Clase para implementar threads de música
class MyThread extends Thread {
  Minim minim;
  AudioPlayer player;
  boolean quit;
  boolean playNow;

  MyThread( Minim m, AudioPlayer p ) {
    minim = m;
    player = p;
    quit    = false;
    playNow = false;
  }

  // Si queremos reproducir
  public void playNow() {
    playNow = true;
  }

  // Si queremos parar la música
  public void quit() {
    quit = true;
    player.pause();
  }

  void run() {
    while ( !quit ) {
      // Esperamos 25ms para no sobrecargar la CPU
      try {
        Thread.sleep(25);
      } 
      catch ( InterruptedException e ) {
        return;
      }

      // Si playNow es TRUE, reproducimos
      while (playNow && !quit) {
        player.play();

        // Espera activa para reiniciar la canción cuando acaba
        while (player.position() < player.length() && !quit) {
          if (player.position() % 100 == 0) {
            println("Reproducidos " + player.position() + " de " + player.length());
          }
        }

        // Reiniciamos la canción
        player.rewind();
      }
    }
  }
}
