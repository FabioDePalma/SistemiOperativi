#include "header.h"

#define LOCK \
  semaforo.sem_num = 2; \
  semaforo.sem_op = -1; \
  semop(id_sem_n, &semaforo, 1);
#define UNLOCK \
  semaforo.sem_num = 2; \
  semaforo.sem_op = 1; \
  semop(id_sem_n, &semaforo, 1);

/***************************************************************************/
/*STRUTTURA DI CIASCUN STUDENTE*/
typedef struct{
  int matricola; /*PID DEL PROCESSO*/
  int pd; /*SE 0 PARI, SE 1 DISPARI*/
  int votoAdE;
  int votoSO;
  int nof_elem; /*PREFERENZA COMPONENTI GRUPPO*/
  int nof_invites;
  int max_reject;
  int schiavo;
  int sono_in_un_gruppo;
  int index_shm_gruppo; /*INDICE AL QUALE DI TROVA IL GRUPPO DEL PROCESSO INTERESSATO*/
  int indice_vettori_gruppo; /*INDICE DEI VETTORI, MATRICOLA E VOTI, ALL'INTERNO DEL VETTORE IN SHM GRUPPI*/
  int capo; /*STUDENTE CAPOGRUPPO */
}student;

/*STRUTTURA SCAMBIO MESSAGGI*/
typedef struct messaggio{
  long mtype;
  int votoArchi;
  int pid;
}Invito;



int main(int argc, char *argv[]) {
  student studente;
  struct sembuf semaforo;
  int id_sem_n, id_sem_f,id_msg_pari, id_msg_dispari;
  int chiave_n, chiave_f;
  int shared_i, id_info_shm, id_shm_gruppi;
  sigset_t my_mask;
  Insieme *insieme_gruppi;
  Info *vettore_shm_votoAdE;
  Invito msg_student_snd;
  Invito msg_student_rcv;
  Invito accetto;
  struct msqid_ds queue_stat;
  struct sigaction action;
  int i, j, k;

  /*SEMAFORO N*/
  chiave_n = atoi(argv[0]);
  if((id_sem_n  = semget(chiave_n, 4, 0666)) == -1){
    printf("Problema semaforo N figli ");
    exit(EXIT_FAILURE);
  }
  semaforo.sem_flg = 0;

  /*ATTACH DELLA MEMORIA CONDIVISA DEL PADRE VOTO ARCHI*/
  id_info_shm = shmget(CHIAVE_SHM, sizeof(*vettore_shm_votoAdE), 0666);
  vettore_shm_votoAdE = (struct info*)shmat(id_info_shm, NULL, 0);

  /*ATTACH DELLA MEMORIA CONDIVISA CODA DI MESSAGGI*/
  id_shm_gruppi = shmget(CHIAVE_GRUPPI, sizeof(*insieme_gruppi), 0666);
  insieme_gruppi = (struct insieme*)shmat(id_shm_gruppi, NULL, 0);

  /***********INIZIALIZZAZIONE INFO STUDENTI *****************/
  printf("inizializzo struttura PID:%d \n", getpid());

  /*ATTACH DELL STUDENT PER DARE LE SUE INFO*/
  LOCK;
  studente.matricola = getpid();
  studente.pd = getpid() % 2;
  studente.votoAdE = voto_architettura();
  studente.nof_invites = atoi(argv[2]);
  studente.max_reject = atoi(argv[3]);
  studente.nof_elem = preferenza();
  studente.schiavo = 0;
  studente.votoSO = 0;
  studente.capo = 0;
  studente.sono_in_un_gruppo = 0; /*NON SONO IN UN GRUPPO (0)*/  /*SONO IN UN GRUPPO (1)*/
  msg_student_snd.mtype = studente.nof_elem;
  msg_student_snd.votoArchi = studente.votoAdE;
  msg_student_snd.pid = studente.matricola;
  accetto.pid = studente.matricola;

  /*SCRITTURA DEL PROPRIO VOTO IN MEMORIA SHM VETTORE VOTI ARCHITETTURA */
  shared_i = vettore_shm_votoAdE->cur_idx;
  vettore_shm_votoAdE->voto_architettura[shared_i] = studente.votoAdE;
  vettore_shm_votoAdE->cur_idx++;
  shmdt(vettore_shm_votoAdE);

  UNLOCK;

/*IL FIGLIO SI ASSOCIA AL SEMAFORO DEI FRATELLI sem_f*/
 chiave_f = atoi(argv[1]);
 if((id_sem_f = semget(chiave_f, 2, 0666)) == -1){
   printf("Problema semaforo F figli ");
   exit(EXIT_FAILURE);
 }

/*IL FIGLIO TERMINA INIZIALIZZAZIONE E AVVISA PADRE*/
 semaforo.sem_num = 0;
 semaforo.sem_op = 1;
 semop(id_sem_f, &semaforo, 1);

/*ATTESA DEL PADRE PER INIZIARE LO SCAMBIO DI MESSAGGI*/
 semaforo.sem_num = 0;
 semaforo.sem_op = 0;
 semop(id_sem_n, &semaforo, 1);

  /*FINE PARTE A DEL CODICE*/
/********************************************************************************************************************/
  /*INIZIO PARTE B DEL CODICE*/
  studente.index_shm_gruppo = -1;/*studente.index_shm_gruppo INIZIALIZZATO A -1 PER NON INCORRERE IN SEGMENTATION FAULT (CORE DUMPED)*/
  studente.indice_vettori_gruppo = 0; /*SCRIVERE NEI VETTORI PID E VOTO DAL PRIMO ELEMENTO*/

  /*COMUNICAZIONE TRA PROCESSI*/
  while(insieme_gruppi->fine == 0 && studente.schiavo == 0){

    if(studente.pd == 0){

      id_msg_pari = msgget(CHIAVE_MSG_PARI, 0666);
      msgctl(id_msg_pari, IPC_STAT, &queue_stat);

      if(queue_stat.msg_qnum == 0 && studente.nof_invites > 0 && studente.sono_in_un_gruppo == 0 && studente.votoAdE >= 24){
        msgsnd(id_msg_pari, &msg_student_snd, sizeof(msg_student_snd)-sizeof(long), 0);/*INVITO*/
        LOCK;
        studente.nof_invites--;
        /*DIVENTO CAPO GRUPPO DEVO SCRIVERE IL MIO PID NELLA MIA STRUTURA GRUPPO IN SHM*/
        studente.index_shm_gruppo = insieme_gruppi->indice;
        insieme_gruppi->gruppo[studente.index_shm_gruppo].pid[studente.indice_vettori_gruppo] = studente.matricola;
        /*METTO IL MIO VOTO E INIZIALIZZO A_C(APERTO_CHIUSO)*/
        insieme_gruppi->gruppo[studente.index_shm_gruppo].voti[studente.indice_vettori_gruppo] = studente.votoAdE;
        insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 0;/*IL GRUPPO È ANCORA APERTO*/
        insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem = studente.nof_elem;
        insieme_gruppi->indice++;/*INCREMENTO INDICE PER IL PROSSIMO CHE CREERÀ IL GRUPPO*/
        studente.indice_vettori_gruppo++;/*INCREMENTO INDICE DEL MIO VETTORE NEL GRUPPO*/
        studente.sono_in_un_gruppo = 1;
        insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem--;
        studente.capo = 1;
        studente.nof_elem = studente.nof_elem -2;
        UNLOCK;

      }else{
        if(studente.sono_in_un_gruppo == 0){ /*SONO AL PRIMO INVITO E NON FACCIO PARTE DI UN GRUPPO*/

          if(msgrcv(id_msg_pari, &msg_student_rcv, sizeof(msg_student_rcv)-sizeof(long), studente.nof_elem, IPC_NOWAIT) != -1){
            /*TOGLIE IL MESSAGGIO DALLA CODA E FA IL CONTROLLO*/
            if(studente.votoAdE <= 24 && msg_student_rcv.votoArchi >= 26){ /*ACCETTO SE HO VOTO < 25*/

              accetto.mtype = msg_student_rcv.pid;/*SALVO, IN MTYPE, IL PID DI CHI HA INVIATO IL MESSAGGIO*/
              accetto.pid = studente.matricola;/*SALVO IN PID, LA MATRICOLA DI CHI ACCETTA*/
              accetto.votoArchi = studente.votoAdE;/*SALVO IN VOTOARCHI, IL VOTO DI CHI ACCETTA*/
              msgsnd(id_msg_pari, &accetto, sizeof(accetto)-sizeof(long), 0); /*INVIO RISPOSTA: ACCETTO*/
              studente.sono_in_un_gruppo = 1;
              studente.nof_invites = 0;
              studente.schiavo = 1;

            }else if(studente.votoAdE >= 26){
              accetto.mtype = msg_student_rcv.pid;/*SALVO, IN MTYPE, IL PID DI CHI HA INVIATO INSIEME_GRUPPI->GRUPPO[STUDENTE.INDEX_SHM_GRUPPO].A_C = 0 MESSAGGIO*/
              accetto.pid = studente.matricola;/*SALVO IN PID, LA MATRICOLA DI CHI ACCETTA*/
              accetto.votoArchi = studente.votoAdE;/*SALVO IN VOTOARCHI, IL VOTO DI CHI ACCETTA*/
              msgsnd(id_msg_pari, &accetto, sizeof(accetto)-sizeof(long), 0); /*INVIO RISPOSTA: ACCETTO*/
              studente.sono_in_un_gruppo = 1;
              studente.nof_invites = 0;
              studente.schiavo = 1;

            }else if (studente.max_reject == 0){/*LIMITE MAX_REJECT (ACCETTO)*/
              accetto.mtype = msg_student_rcv.pid;/*SALVO, IN MTYPE, IL PID DI CHI HA INVIATO IL MESSAGGIO*/
              accetto.pid = studente.matricola;/*SALVO IN PID, LA MATRICOLA DI CHI ACCETTA*/
              accetto.votoArchi = studente.votoAdE;/*SALVO IN VOTOARCHI, IL VOTO DI CHI ACCETTA*/
              msgsnd(id_msg_pari, &accetto, sizeof(accetto)-sizeof(long), 0); /*INVIO RISPOSTA: ACCETTO*/
              studente.sono_in_un_gruppo = 1;
              studente.nof_invites = 0;
              studente.schiavo = 1;

            }else if(studente.max_reject > 0){/*RIFIUTO E METTO DI NUOVO IL MESSAGGIO IN CODA*/
              studente.max_reject--;
              msgsnd(id_msg_pari, &msg_student_rcv, sizeof(msg_student_rcv)-sizeof(long), 0);
            }
          }else if(studente.nof_invites > 0 && studente.votoAdE >= 22){/*MSGRCV == -1 NON CI SONO INVITI DEL MIO TIPO*/
            msgsnd(id_msg_pari, &msg_student_snd, sizeof(msg_student_snd)-sizeof(long), 0);/*INVITO*/
            LOCK;
            /*DIVENTO CAPO GRUPPO E DEVO SCRIVERE IL MIO PID NELLA MIA STRUTURA GRUPPO IN SHM*/
            studente.nof_invites--;
            studente.index_shm_gruppo = insieme_gruppi->indice;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].pid[studente.indice_vettori_gruppo] = studente.matricola;
            /*METTO IL MIO VOTO E INIZIALIZZO A_C(APERTO_CHIUSO)*/
            insieme_gruppi->gruppo[studente.index_shm_gruppo].voti[studente.indice_vettori_gruppo] = studente.votoAdE;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 0;/*IL GRUPPO È ANCORA APERTO*/
            insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem = studente.nof_elem;
            insieme_gruppi->indice++;/*INCREMENTO INDICE PER IL PROSSIMO CHE CREERÀ IL GRUPPO*/
            studente.indice_vettori_gruppo++;/*INCREMENTO INDICE DEL MIO VETTORE NEL GRUPPO*/
            studente.sono_in_un_gruppo = 1;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem--;
            studente.capo = 1;
            studente.nof_elem = studente.nof_elem -2;
            UNLOCK;
          }
        }else if(studente.nof_invites >= 0){/*QUI CI ENTRANO I CAPOGRUPPO*/
          if(insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem != 0 && msgrcv(id_msg_pari, &accetto, sizeof(accetto)-sizeof(long), studente.matricola, IPC_NOWAIT) != -1){
            /*SE IL TARGET DEL GRUPPO È DIVERSO DA 0 E CI SONO RISPOSTE AI MIEI INVITI*/
            /*SCRIVI NUOVO MEMBRO NEL TUO GRUPPO CHE HAI GIÀ CREATO*/
            LOCK;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].pid[studente.indice_vettori_gruppo] = accetto.pid;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].voti[studente.indice_vettori_gruppo] = accetto.votoArchi;
            studente.indice_vettori_gruppo++;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem--;
            UNLOCK ;
          }else if(studente.capo == 1 && studente.nof_invites > 0 && studente.nof_elem > 0 && insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem > 0){/*sono un capogruppo e devo invitare*/
            msgsnd(id_msg_pari, &msg_student_snd, sizeof(msg_student_snd)-sizeof(long), 0);/*inviti*/
            studente.nof_invites--;
            studente.nof_elem--;

          }else if(studente.capo == 1 && insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem == 0){
            insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 1;
            studente.schiavo = 1;/*CAPOGRUPPO DIVENTA SCHIAVO PERCHÈ CHIUDE IL GRUPPO ED ESCE DEL WHILE*/
          }
        }
      }

      if(msgctl(id_msg_pari, IPC_STAT, &queue_stat) == -1 && studente.capo == 1){
        insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 1;
      }
    }else{ /*CODA DISPARI*/
      id_msg_dispari = msgget(CHIAVE_MSG_DISPARI, 0666);
      msgctl(id_msg_dispari, IPC_STAT, &queue_stat);

      if(queue_stat.msg_qnum == 0 && studente.nof_invites > 0 && studente.sono_in_un_gruppo == 0 && studente.votoAdE >= 24){
        msgsnd(id_msg_dispari, &msg_student_snd, sizeof(msg_student_snd)-sizeof(long), 0);/*INVITO*/
        LOCK;
        studente.nof_invites--;
        /*DIVENTO CAPO GRUPPO DEVO SCRIVERE IL MIO PID NELLA MIA STRUTURA GRUPPO IN SHM*/
        studente.index_shm_gruppo = insieme_gruppi->indice;
        insieme_gruppi->gruppo[studente.index_shm_gruppo].pid[studente.indice_vettori_gruppo] = studente.matricola;
        /*METTO IL MIO VOTO E INIZIALIZZO A_C(APERTO_CHIUSO)*/
        insieme_gruppi->gruppo[studente.index_shm_gruppo].voti[studente.indice_vettori_gruppo] = studente.votoAdE;
        insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 0;/*IL GRUPPO È ANCORA APERTO*/
        insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem = studente.nof_elem;
        insieme_gruppi->indice++;/*INCREMENTO INDICE PER IL PROSSIMO CHE CREERÀ IL GRUPPO*/
        studente.indice_vettori_gruppo++;/*INCREMENTO INDICE DEL MIO VETTORE NEL GRUPPO*/
        studente.sono_in_un_gruppo = 1;
        insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem--;
        studente.capo = 1;
        studente.nof_elem = studente.nof_elem -2;
        UNLOCK;

      }else{
        if(studente.sono_in_un_gruppo == 0){ /*SONO AL PRIMO INVITO E NON FACCIO PARTE DI UN GRUPPO*/

          if(msgrcv(id_msg_dispari, &msg_student_rcv, sizeof(msg_student_rcv)-sizeof(long), studente.nof_elem, IPC_NOWAIT) != -1){
            /*TOGLIE IL MESSAGGIO DALLA CODA E FA IL CONTROLLO*/
            if(studente.votoAdE <= 24 && msg_student_rcv.votoArchi >= 26){ /*ACCETTO SE HO VOTO < 25*/

              accetto.mtype = msg_student_rcv.pid;/*SALVO, IN MTYPE, IL PID DI CHI HA INVIATO IL MESSAGGIO*/
              accetto.pid = studente.matricola;/*SALVO IN PID, LA MATRICOLA DI CHI ACCETTA*/
              accetto.votoArchi = studente.votoAdE;/*SALVO IN VOTOARCHI, IL VOTO DI CHI ACCETTA*/
              msgsnd(id_msg_dispari, &accetto, sizeof(accetto)-sizeof(long), 0); /*INVIO RISPOSTA: ACCETTO*/
              studente.sono_in_un_gruppo = 1;
              studente.nof_invites = 0;
              studente.schiavo = 1;

            }else if(studente.votoAdE >= 26){
              accetto.mtype = msg_student_rcv.pid;/*SALVO, IN MTYPE, IL PID DI CHI HA INVIATO INSIEME_GRUPPI->GRUPPO[STUDENTE.INDEX_SHM_GRUPPO].A_C = 0 MESSAGGIO*/
              accetto.pid = studente.matricola;/*SALVO IN PID, LA MATRICOLA DI CHI ACCETTA*/
              accetto.votoArchi = studente.votoAdE;/*SALVO IN VOTOARCHI, IL VOTO DI CHI ACCETTA*/
              msgsnd(id_msg_dispari, &accetto, sizeof(accetto)-sizeof(long), 0);/*INVIO RISPOSTA: ACCETTO*/
              studente.sono_in_un_gruppo = 1;
              studente.nof_invites = 0;
              studente.schiavo = 1;

            }else if (studente.max_reject == 0){/*LIMITE MAX_REJECT (ACCETTO)*/
              accetto.mtype = msg_student_rcv.pid;/*SALVO, IN MTYPE, IL PID DI CHI HA INVIATO IL MESSAGGIO*/
              accetto.pid = studente.matricola;/*SALVO IN PID, LA MATRICOLA DI CHI ACCETTA*/
              accetto.votoArchi = studente.votoAdE;/*SALVO IN VOTOARCHI, IL VOTO DI CHI ACCETTA*/
              msgsnd(id_msg_dispari, &accetto, sizeof(accetto)-sizeof(long), 0); /*INVIO RISPOSTA: ACCETTO*/
              studente.sono_in_un_gruppo = 1;
              studente.nof_invites = 0;
              studente.schiavo = 1;
            }else if(studente.max_reject > 0){/*RIFIUTO E METTO DI NUOVO IL MESSAGGIO IN CODA*/
              studente.max_reject--;
              msgsnd(id_msg_dispari, &msg_student_rcv, sizeof(msg_student_rcv)-sizeof(long), 0);
            }
          }else if(studente.nof_invites > 0 && studente.votoAdE >= 22){/*MSGRCV == -1 NON CI SONO INVITI DEL MIO TIPO*/
            msgsnd(id_msg_dispari, &msg_student_snd, sizeof(msg_student_snd)-sizeof(long), 0);/*INVITO*/
            LOCK;
            /*DIVENTO CAPO GRUPPO DEVO SCRIVERE IL MIO PID NELLA MIA STRUTURA GRUPPO IN SHM*/
            studente.nof_invites--;
            studente.index_shm_gruppo = insieme_gruppi->indice;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].pid[studente.indice_vettori_gruppo] = studente.matricola;
            /*METTO IL MIO VOTO E INIZIALIZZO A_C(APERTO_CHIUSO)*/
            insieme_gruppi->gruppo[studente.index_shm_gruppo].voti[studente.indice_vettori_gruppo] = studente.votoAdE;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 0;/*IL GRUPPO È ANCORA APERTO*/
            insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem = studente.nof_elem;
            insieme_gruppi->indice++;/*INCREMENTO INDICE PER IL PROSSIMO CHE CREERÀ IL GRUPPO*/
            studente.indice_vettori_gruppo++;/*INCREMENTO INDICE DEL MIO VETTORE NEL GRUPPO*/
            studente.sono_in_un_gruppo = 1;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem--;
            studente.capo = 1;
            studente.nof_elem = studente.nof_elem -2;
            UNLOCK;
          }
        }else if(studente.nof_invites >= 0){/*QUI CI ENTRANO I CAPOGRUPPO*/
          if(insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem != 0 && msgrcv(id_msg_dispari, &accetto, sizeof(accetto)-sizeof(long), studente.matricola, IPC_NOWAIT) != -1){
            /*SE IL TARGET DEL GRUPPO È DIVERSO DA 0 E CI SONO RISPOSTE AI MIEI INVITI*/
            /*SCRIVI NUOVO MEMBRO NEL TUO GRUPPO CHE HAI GIÀ CREATO*/
            LOCK;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].pid[studente.indice_vettori_gruppo] = accetto.pid;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].voti[studente.indice_vettori_gruppo] = accetto.votoArchi;
            studente.indice_vettori_gruppo++;
            insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem--;
            UNLOCK ;
          }else if(studente.capo == 1 && studente.nof_invites > 0 && studente.nof_elem > 0 && insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem > 0){/*sono un capogruppo e devo invitare*/
            msgsnd(id_msg_dispari, &msg_student_snd, sizeof(msg_student_snd)-sizeof(long), 0);/*inviti*/
            studente.nof_invites--;
            studente.nof_elem--;

          }else if(studente.capo == 1 && insieme_gruppi->gruppo[studente.index_shm_gruppo].nof_elem == 0){
            insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 1;
            studente.schiavo = 1;/*CAPOGRUPPO DIVENTA SCHIAVO PERCHÈ CHIUDE IL GRUPPO ED ESCE DEL WHILE*/
          }
        }
      }

      if(msgctl(id_msg_dispari, IPC_STAT, &queue_stat) == -1 && studente.capo == 1){
        insieme_gruppi->gruppo[studente.index_shm_gruppo].a_c = 1;
      }
    }
  }


  /*ASPETTANO CHE IL PADRE ABBIA FINITO DI SCRIVERE IN SHM GRUPPI*/
  semaforo.sem_num = 3;
  semaforo.sem_op = 0;
  semop(id_sem_n, &semaforo, 1);

  /*CERCANO IL PROPRIO VOTO*/
  for(i = 0; i < insieme_gruppi->indice; i++){
    for(j = 0; j < 4; j++){
      if(studente.matricola == insieme_gruppi->gruppo[i].pid[j]){
        studente.votoSO = insieme_gruppi->gruppo[i].voti[j];
      }
    }
  }

  /*STAMPANO LE PROPRIO INFORMAZIONI*/
  fflush(stdout);
  printf("matricola: %d, voto Architettura: %d, voto Sistemi Operativi: %d\n",studente.matricola, studente.votoAdE, studente.votoSO);



  /*DA IL VIA AL PADRE PER STAMPARE LE STATISTICHE E TERMINA*/
  semaforo.sem_num = 1;
  semaforo.sem_op = 1;
  semop(id_sem_f, &semaforo, 1);

  exit(EXIT_SUCCESS);

}/*END MAIN*/




int voto_architettura(){
  struct timespec now;
  int voto;
  clock_gettime(CLOCK_REALTIME, &now);
  voto = (now.tv_nsec % 13) + 18;
  return voto;
}

int preferenza(){
  int numero, valore;
  int id_shm_urna;
  int*index_shm_urna;
  struct timespec now;
  id_shm_urna = shmget(CHIAVE_URNA, sizeof(int)*100, 0);
  index_shm_urna = (int *)shmat(id_shm_urna, NULL, SHM_RDONLY);
  clock_gettime(CLOCK_REALTIME, &now);
  numero = now.tv_nsec % 99;
  valore = index_shm_urna[numero];
  shmdt(index_shm_urna);
  return valore;
}
