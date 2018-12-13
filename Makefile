#COMPILER=arm-linux-gnueabihf-

all:
	$(COMPILER)gcc -o playwav main.c wav_player.c -I/usr/include -L/usr/lib -lasound

lib:
	$(COMPILER)gcc -c wav_player.c -I/usr/include -L/usr/lib -lasound -lpthread
	$(COMPILER)ar -crs libplaywav.a *.o

clean:
	-rm ./*.o
	-rm ./playwav
	-rm ./libplaywav.a
