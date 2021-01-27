import ddf.minim.spi.*;
import ddf.minim.signals.*;
import ddf.minim.*;
import ddf.minim.analysis.*;
import ddf.minim.ugens.*;
import ddf.minim.effects.*;

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

  public void playNow() {
    playNow = true;
  }

  public void quit() {
    quit = true;
    player.pause();
  }

  void run() {
    while ( !quit ) {
      // wait 10 ms, then check if need to play
      try {
        Thread.sleep( 25 );
      } 
      catch ( InterruptedException e ) {
        return;
      }

      println("Estado playNow: " + playNow);

      // if we have to play the sound, do it!
      while (playNow && !quit) {
        player.play();

        while (player.position() < player.length() && !quit) {
          println("Reproducidos " + player.position() + " de " + player.length());
        }
        
        println("He salido del while");

        player.rewind();

        //player.rewind();
      }

      // go back and wait again for 10 ms...
    }
  }
}
