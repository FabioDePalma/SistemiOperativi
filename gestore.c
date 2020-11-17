#include "header.h"

#define CHIAVE_N 1234
#define CHIAVE_F 4321

int main() {
  char **argv = malloc(5*sizeof(char*));
  char* chiave_n = (char*)malloc(sizeof(char)*5);/*CHIAVE PER L'ATTESA NASCITA FIGLI -SEMAFORO N*/
  char* chiave_f = (char*)malloc(sizeof(char)*5);/*CHIAVE PER L'ATTESA DEI FRATELLI -SEMAFOTO F*/
  char* invites = (char*)malloc(sizeof(char)*4);/*CI SERVE PER PRENDERE IL N DI INVITI DAL OPT.CONF*/
  char* reject = (char*)malloc(sizeof(char)*4);/*CI SERVE PER PRENDERE IN N DI RIFIUTI DAL OPT.CONF*/

  struct sembuf semaforo;
  struct sigaction action;
  Insieme *insieme_gruppi;
  sigset_t my_mask;

  pid_t pid;
  int id_shm, id_sem_n, id_sem_f, id_info_shm, id_shm_gruppi, id_msg_pari, id_msg_dispari;
  int nof_invites, max_reject;
  int indice_stampa_vettori, i, j, indice_stampa_gruppi;
  int mediaAdE, mediaSO, valoreMax;
  int *puntatore_mem;
  Info *vettore_shm_votoAdE;

  /*VETTORE STATISTICHE*/
  int indice_statistica, totale;
  int n_persone_voti_so[14] = {0};
  int n_persone_voti_ade[14] = {0};

  /*VETTORE PERCENTUALI*/
  FILE* fp;
  int v[100] = {0};
  int percentuali[3]; /*prendere da file opt.conf*/
  int o;
  int *urna;
  urna = puntatore_mem;

  fp = fopen("opt.conf", "r");
  fscanf(fp,"percentuale2 =%d\n percentuale3 =%d\n percentuale4 =%d\n nof_invites =%d\n max_reject =%d\n", &percentuali[0], &percentuali[1], &percentuali[2], &nof_invites, &max_reject);
  fclose(fp);

  /*STAMPA PID DEL PADRE*/
  printf("PID: %d\n", getpid());

  /*CREAZIONE MEMORIA CONDIVISA PER URNA*/
  id_shm = shmget(CHIAVE_URNA, sizeof(int)*100, IPC_CREAT | 0666);
  if(id_shm == -1){
    printf("errore creazione area di memoria condivisa");
    TEST_ERROR;
    exit(EXIT_FAILURE);
  }
  puntatore_mem = shmat(id_shm, NULL, 0);
  urna = make_urna(percentuali, (int *)shmat(id_shm, NULL, 0));


  /*CREAZIONE MEMORIA CONDIVISA GRUPPI*/
  id_shm_gruppi = shmget(CHIAVE_GRUPPI, sizeof(Insieme),IPC_CREAT | 0666);
  if(id_shm_gruppi == -1){
    printf("errore creazione area di memoria condivisa");
    TEST_ERROR;
    exit(EXIT_FAILURE);
  }
  insieme_gruppi = shmat(id_shm_gruppi, NULL, 0);
  insieme_gruppi->indice = 0;
  insieme_gruppi->fine = 0;

  /*CREAZIONE MEMORIA CONDIVISA SHM_INFO PER LA MEDIA DEL VOTO DI ARCHITETTURA*/
  id_info_shm = shmget(CHIAVE_SHM, sizeof(Info), IPC_CREAT | 0666);
  if(id_info_shm == -1){
    printf("errore creazione area di memoria condivisa");
    TEST_ERROR;
    exit(EXIT_FAILURE);
  }
  vettore_shm_votoAdE = shmat(id_info_shm, NULL, 0);
  vettore_shm_votoAdE->cur_idx = 0;

  /*CREAZIONE SEMAFORI*/
  if((id_sem_n = semget(CHIAVE_N, 4, IPC_CREAT | IPC_EXCL | 0666)) == -1){
		printf("Problema semaforo N ");
		exit(EXIT_FAILURE);
  }

  if((id_sem_f = semget(CHIAVE_F, 2, IPC_CREAT | IPC_EXCL | 0666)) == -1){
    printf("Problema semaforo F ");
    exit(EXIT_FAILURE);
  }

  /*CONVERSIONE DA INTERO A STRINGA PER ARGV*/
  sprintf(chiave_n, "%d", CHIAVE_N);
  sprintf(chiave_f, "%d", CHIAVE_F);
  sprintf(invites, "%d", nof_invites);
  sprintf(reject, "%d", max_reject);

  /*SALVATAGGIO CHIAVE IN ARGV*/
  argv[0] = chiave_n;
  argv[1] = chiave_f;
  argv[2] = invites;
  argv[3] = reject;
  argv[4] = NULL;

  /*INIZIALIZZAZIONE SEMAFORI*/
  semctl(id_sem_n, 0, SETVAL, 1);
  semctl(id_sem_n, 1, SETVAL, 1);
  semctl(id_sem_n, 2, SETVAL, 1);
  semctl(id_sem_n, 3, SETVAL, 1);
  semctl(id_sem_f, 0, SETVAL, 0);
  semctl(id_sem_f, 1, SETVAL, 0);

  semaforo.sem_flg = 0;

  /*CREAZIONE CODE DI MESSAGGI*/
  id_msg_pari = msgget(CHIAVE_MSG_PARI, IPC_CREAT | IPC_EXCL | 0666);
  if(id_msg_pari == -1){
    printf("errore creazione coda\n" );
    TEST_ERROR;
  }
  id_msg_dispari = msgget(CHIAVE_MSG_DISPARI, IPC_CREAT | IPC_EXCL | 0666);
  if(id_msg_dispari == -1){
    printf("errore creazione coda\n" );
    TEST_ERROR;
  }
/*errno = 0;*/
  /*CREAZIONE FIGLI*/
  for(i = 0; i < POP_SIZE; i++){
    switch(pid = fork()){
      case 0:
        /*FIGLIO*/
        execve("student", argv, NULL);
        /*qui lo studente non ci deve mai tornare*/
        TEST_ERROR;
        break;
      default:
        TEST_ERROR;
        break;
    }
  }

  /*ATTESA DELLO 0 IN id_sem_f(padre aspetta inizializzazione della struttura dei figli)*/
  semaforo.sem_num = 0;
  semaforo.sem_op = -POP_SIZE;
  semop(id_sem_f, &semaforo, 1);

  /*INIZIALIZZO SIGACTION & MASK*/
  action.sa_handler = fine_tempo;
  sigemptyset(&my_mask);
  action.sa_mask = my_mask;
  action.sa_flags = 0;
  sigaction(SIGALRM, &action, NULL);
  printf("via\n");
  alarm(SIM_TIME);

  /*INIZIO TEMPO SIM_TIME DA QUESTO PUNTO IN POI INIZIERÀ LA SIMULAZIONE*/
  /*IL PADRE DA IL VIA AI FIGLI, I FIGLI INIZIANO A COMUNICARE TRA DI LORO*/
  semaforo.sem_num = 0;
  semaforo.sem_op = -1;
  semop(id_sem_n, &semaforo, 1);

  /*IL PADRE SI ADDORMENTA IN ATTESA DI ESSERE RISVEGLIATO DAL SIM TIME*/
  semaforo.sem_num = 1;
  semaforo.sem_op = 0;
  semop(id_sem_n, &semaforo, 1);

  printf("Continuo\n");
  /*AL RISVEGLIO DEL PADRE - IMPOSTA LA VARIABILE FINE A 1
  COSÌ DA IMPEDIRE AI FIGLI DI ENTRARE ULTERIORMENTE NEL WHILE */
  insieme_gruppi->fine = 1;


  /*RIMOZIONE SHM "URNA"*/
  shmctl(id_shm, IPC_RMID, NULL);
/*STAMPA DEI GRUPPI*/
  for(indice_stampa_gruppi = 0; indice_stampa_gruppi < insieme_gruppi->indice; indice_stampa_gruppi++){
    fflush(stdout);
    printf("gruppo ac: %d", insieme_gruppi->gruppo[indice_stampa_gruppi].a_c);
    printf(" gruppo nof_elem: %d\n", insieme_gruppi->gruppo[indice_stampa_gruppi].nof_elem);
    for(indice_stampa_vettori = 0; indice_stampa_vettori < 4; indice_stampa_vettori++){
      printf("PID: %d\n", insieme_gruppi->gruppo[indice_stampa_gruppi].pid[indice_stampa_vettori]);
      printf("voti: %d\n", insieme_gruppi->gruppo[indice_stampa_gruppi].voti[indice_stampa_vettori]);

    }
    printf("\n");
  }

  /*MEDIA VOTO ARCHITETTURA*/
  mediaAdE = 0;
  for(i = 0; i < POP_SIZE; i++){
    mediaAdE = mediaAdE + vettore_shm_votoAdE->voto_architettura[i];

    switch (vettore_shm_votoAdE->voto_architettura[i]) {
      case 18:
        n_persone_voti_ade[1]= n_persone_voti_ade[1] + 1;
        break;
      case 19:
        n_persone_voti_ade[2]= n_persone_voti_ade[2] + 1;
        break;
      case 20:
        n_persone_voti_ade[3]= n_persone_voti_ade[3] + 1;
        break;
      case 21:
        n_persone_voti_ade[4]= n_persone_voti_ade[4] + 1;
        break;
      case 22:
        n_persone_voti_ade[5]= n_persone_voti_ade[5] + 1;
        break;
      case 23:
        n_persone_voti_ade[6]= n_persone_voti_ade[6] + 1;
        break;
      case 24:
        n_persone_voti_ade[7]= n_persone_voti_ade[7] + 1;
        break;
      case 25:
        n_persone_voti_ade[8]= n_persone_voti_ade[8] + 1;
        break;
      case 26:
        n_persone_voti_ade[9]= n_persone_voti_ade[9] + 1;
        break;
      case 27:
        n_persone_voti_ade[10]= n_persone_voti_ade[10] + 1;
        break;
      case 28:
        n_persone_voti_ade[11]= n_persone_voti_ade[11] + 1;
        break;
      case 29:
        n_persone_voti_ade[12]= n_persone_voti_ade[12] + 1;
        break;
      case 30:
        n_persone_voti_ade[13]= n_persone_voti_ade[13] + 1;
        break;
      default:
        break;
    }
  }
  mediaAdE = mediaAdE / POP_SIZE;

  /* METTE IN MEMORIA I VOTI E FA LA MEDIA VOTO SISTEMI OPERATIVI*/
  mediaSO = 0;
  for(i = 0; i < insieme_gruppi->indice; i++){
    valoreMax = 0;
    if(insieme_gruppi->gruppo[i].a_c == 1){
      valoreMax = insieme_gruppi->gruppo[i].voti[0];
      for(j = 1; j < 4 && insieme_gruppi->gruppo[i].voti[j] != 0 ; j++){
        if(valoreMax < insieme_gruppi->gruppo[i].voti[j]){
          valoreMax = insieme_gruppi->gruppo[i].voti[j];
        }
      }
      if(insieme_gruppi->gruppo[i].nof_elem != 0){
        valoreMax = valoreMax-3;
      }
      for(o = 0; o < j; o++){
        insieme_gruppi->gruppo[i].voti[o] = valoreMax;
      }
    }
    mediaSO = mediaSO + (valoreMax*j);

    switch (valoreMax) {
      case 18:
        n_persone_voti_so[1]= n_persone_voti_so[1] + (j);
        break;
      case 19:
        n_persone_voti_so[2]= n_persone_voti_so[2] + (j);
        break;
      case 20:
        n_persone_voti_so[3]= n_persone_voti_so[3] + (j);
        break;
      case 21:
        n_persone_voti_so[4]= n_persone_voti_so[4] + (j);
        break;
      case 22:
        n_persone_voti_so[5]= n_persone_voti_so[5] + (j);
        break;
      case 23:
        n_persone_voti_so[6]= n_persone_voti_so[6] + (j);
        break;
      case 24:
        n_persone_voti_so[7]= n_persone_voti_so[7] + (j);
        break;
      case 25:
        n_persone_voti_so[8]= n_persone_voti_so[8] + (j);
        break;
      case 26:
        n_persone_voti_so[9]= n_persone_voti_so[9] + (j);
        break;
      case 27:
        n_persone_voti_so[10]= n_persone_voti_so[10] + (j);
        break;
      case 28:
        n_persone_voti_so[11]= n_persone_voti_so[11] + (j);
        break;
      case 29:
        n_persone_voti_so[12]= n_persone_voti_so[12] + (j);
        break;
      case 30:
        n_persone_voti_so[13]= n_persone_voti_so[13] + (j);
        break;
      default:
        break;
    }

  }
  mediaSO = mediaSO / POP_SIZE;
  totale = 0;
  for(indice_statistica = 1; indice_statistica < 14; indice_statistica ++){
    totale = totale + n_persone_voti_so[indice_statistica];
  }
  n_persone_voti_so[0] = POP_SIZE -totale;

  printf("Ho finito di assegnare i voti\n");
  printf("Aspetto che i figli stampino i loro voti\n");
  /*I FIGLI ASPETTANO CHE IL PADRE ABBIA MESSO TUTTI I VOTI NELLA SHM
    AL TERMINE IL PADRE SBLOCCA IL SEMAFORO AI FIGLI*/
  semaforo.sem_num = 3;
  semaforo.sem_op = -1;
  semop(id_sem_n, &semaforo, 1);

  /*IL PADRE HA ASPETTA LA STAMPA FINALE DEI FIGLI E SI SVEGLIA */
  semaforo.sem_num = 1;
  semaforo.sem_op = -POP_SIZE;
  semop(id_sem_f, &semaforo, 1);

  /*MEDIA VOTI E STATISTICHE*/
  printf("media voto Architettura degli Elaboratori: %d\n", mediaAdE);
  printf("media voto SO: %d\n", mediaSO);
  printf("                              voti:\t AdE\t\tSO\n\n");
  printf("Le persone che hanno preso  0 sono:\t %d\t\t%d\n", n_persone_voti_ade[0], n_persone_voti_so[0]);
  printf("Le persone che hanno preso 18 sono:\t %d\t\t%d\n", n_persone_voti_ade[1], n_persone_voti_so[1]);
  printf("Le persone che hanno preso 19 sono:\t %d\t\t%d\n", n_persone_voti_ade[2], n_persone_voti_so[2]);
  printf("Le persone che hanno preso 20 sono:\t %d\t\t%d\n", n_persone_voti_ade[3], n_persone_voti_so[3]);
  printf("Le persone che hanno preso 21 sono:\t %d\t\t%d\n", n_persone_voti_ade[4], n_persone_voti_so[4]);
  printf("Le persone che hanno preso 22 sono:\t %d\t\t%d\n", n_persone_voti_ade[5], n_persone_voti_so[5]);
  printf("Le persone che hanno preso 23 sono:\t %d\t\t%d\n", n_persone_voti_ade[6], n_persone_voti_so[6]);
  printf("Le persone che hanno preso 24 sono:\t %d\t\t%d\n", n_persone_voti_ade[7], n_persone_voti_so[7]);
  printf("Le persone che hanno preso 25 sono:\t %d\t\t%d\n", n_persone_voti_ade[8], n_persone_voti_so[8]);
  printf("Le persone che hanno preso 26 sono:\t %d\t\t%d\n", n_persone_voti_ade[9], n_persone_voti_so[9]);
  printf("Le persone che hanno preso 27 sono:\t %d\t\t%d\n", n_persone_voti_ade[10], n_persone_voti_so[10]);
  printf("Le persone che hanno preso 28 sono:\t %d\t\t%d\n", n_persone_voti_ade[11], n_persone_voti_so[11]);
  printf("Le persone che hanno preso 29 sono:\t %d\t\t%d\n", n_persone_voti_ade[12], n_persone_voti_so[12]);
  printf("Le persone che hanno preso 30 sono:\t %d\t\t%d\n", n_persone_voti_ade[13], n_persone_voti_so[13]);

  /*FREE DELLE STRUTTURE ALLOCATE DINAMICAMENTE*/
  free(chiave_n);
  free(chiave_f);
  free(invites);
  free(reject);
  free(argv);

  /*RIMOZIONE DEI SEMAFORI E DELLE SHM*/
  semctl(id_sem_n, 0, IPC_RMID);
  semctl(id_sem_f, 0, IPC_RMID);
  shmctl(id_shm_gruppi, IPC_RMID, 0); /*CHIUSURA DELLA SHM "GRUPPI"*/
  shmctl(id_info_shm, IPC_RMID, 0);/*CHIUSURA "SHM_VETTORE_ARCHITETTURA"*/

}/*END MAIN*/

/*HENDLER*/
void fine_tempo(int signal){
  struct sembuf semaforo;
  int id_msg_pari, id_msg_dispari, id_sem_sim_time;
  id_msg_pari = msgget(CHIAVE_MSG_PARI, 0666);
  id_msg_dispari = msgget(CHIAVE_MSG_DISPARI, 0666);
  msgctl(id_msg_pari, IPC_RMID, NULL);
  msgctl(id_msg_dispari, IPC_RMID, NULL);


  id_sem_sim_time = semget(CHIAVE_N, 1, 0666);
  TEST_ERROR
  printf("è scaduto il tempo\n");
}

int *make_urna(int *arr, int *v){
  int count, i, j;
  j = 0;
  count = arr[j];
  for(i = 0; i < 100; i++){
    if(i < count && j == 0){
      v[i] = 2;
      if(i == count-1){
        j++;
        count += arr[j];
      }
    }else if(i < count && j == 1){
      v[i] = 3;
      if(i == count-1){
        j++;
        count += arr[j];
      }
    }else{
      v[i] = 4;
    }
  }
  return v;
}
