#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#define KEYLENGTH 32
/*
 * Decrypt *len bytes of ciphertext.
 * Outputs plaintext used to determine key.
 */
unsigned char *aes_decrypt(EVP_CIPHER_CTX *e, unsigned char *ciphertext, 
			   int *len)
{
	// plaintext will always be <= to the length of ciphertext
	int p_len = *len, f_len = 0;
	unsigned char *plaintext = malloc(p_len);

	EVP_DecryptInit_ex(e, NULL, NULL, NULL, NULL);
	EVP_DecryptUpdate(e, plaintext, &p_len, ciphertext, *len);
	EVP_DecryptFinal_ex(e, plaintext+p_len, &f_len);

	return plaintext;
}
/*
 * Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the 
 * supplied key material.
 * nrounds is the number of times the we hash the material. 
 * More rounds are more secure but slower.
 */
int aes_init(unsigned char *key_data, int key_data_len, 
	     EVP_CIPHER_CTX *e_ctx, EVP_CIPHER_CTX *d_ctx)
{
	unsigned char key[KEYLENGTH], iv[KEYLENGTH];

	// Some robust programming to start with
	// Only use most significant 32 bytes of data if > 32 bytes
	if (key_data_len > KEYLENGTH) key_data_len = KEYLENGTH;

	// Copy bytes to the front of the key array
	int i;
	for (i = 0; i < key_data_len; i++) {
		key[i] = key_data[i];
		iv[i] = key_data[i];
	}
	for (i = key_data_len; i < KEYLENGTH; i++) {
		key[i] = 0;
		iv[i] = 0;
	} 
	//Were not using the key gen process as we assume the key has 
	// already been created in this way.
	EVP_CIPHER_CTX_init(e_ctx);
	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_cbc(), NULL, key, iv);
	EVP_CIPHER_CTX_init(d_ctx);
	EVP_DecryptInit_ex(d_ctx, EVP_aes_256_cbc(), NULL, key, iv);

	return 0;
}
/*
 * Searches for possible remaining bits in the full key.
 */
void trialkey_search(unsigned char *trialkey, unsigned char *key, int *search_byte) 
{
	unsigned long keyLowBits = 0;
	keyLowBits = ((unsigned long)(key[24] & 0xFFFF)<< 56)|
		     ((unsigned long)(key[25] & 0xFFFF)<< 48)|
		     ((unsigned long)(key[26] & 0xFFFF)<< 40)|
		     ((unsigned long)(key[27] & 0xFFFF)<< 32)|
		     ((unsigned long)(key[28] & 0xFFFF)<< 24)|
		     ((unsigned long)(key[29] & 0xFFFF)<< 16)|
		     ((unsigned long)(key[30] & 0xFFFF)<< 8)|
		     ((unsigned long)(key[31] & 0xFFFF));

	unsigned long trialLowBits = keyLowBits | *search_byte;

	trialkey[25] = (unsigned char) (trialLowBits >> 48);
	trialkey[26] = (unsigned char) (trialLowBits >> 40);
	trialkey[27] = (unsigned char) (trialLowBits >> 32);
	trialkey[28] = (unsigned char) (trialLowBits >> 24);
	trialkey[29] = (unsigned char) (trialLowBits >> 16);
	trialkey[30] = (unsigned char) (trialLowBits >> 8);
	trialkey[31] = (unsigned char) (trialLowBits);
}
/*
 * Main search function used by each process.
 * Reads the files cipher.txt, plain.txt,
 * divides the search space depending on process,
 * outputs result of the search: 
 * * Key found = 1
 * * Key not found = 0
 * * Encrypt error = -1
 * * Partial key too long = -2
 */
int search_func(char **argv, int spid, int nprocs, unsigned char buff[])
{	
	char *plaintext;
	char plain_in[4096];
	int  key_data_len = strlen(argv[2]);
	unsigned char *key_data = (unsigned char *)argv[2];
	unsigned char key[KEYLENGTH], iv[KEYLENGTH], trialkey[KEYLENGTH], 
		      cipher_in[4096]; 
	unsigned long maxSpace = 0, part, start, end, search_byte;
	int cipher_length = KEYLENGTH, i, counter;

	// Reads file contents.
	FILE *mycipherfile;    
	mycipherfile=fopen("cipher.txt","r");
	fread(cipher_in, cipher_length, 1, mycipherfile);
	FILE *myplainfile;    
	myplainfile=fopen("plain.txt","r");
	fread(plain_in, 28, 1, myplainfile);

	// Prints file contents.
	if (spid == 1) {
		fprintf(stderr, "Plain: %s\n", plain_in);
		fprintf(stderr, "Cipher: %s\n", cipher_in);
	}

	if(key_data_len > KEYLENGTH) key_data_len = KEYLENGTH;

	// Copy bytes to the front of the key.
	for (i = 0; i < key_data_len; i++) {     
		key[i] = key_data[i];
		iv[i] = key_data[i];
		trialkey[i] = key_data[i];
	}
	// Pad remaining bits with 0s.
	for (i = key_data_len; i < KEYLENGTH; i++) {
		key[i] = 0;
		iv[i] = 0;
		trialkey[i] = 0;
	}
	// Determine maximum search space, return error if < 1, 
	// i.e. partial key >= key.
	maxSpace = ((unsigned long)1 << ((KEYLENGTH - key_data_len)*8))-1;
	if (maxSpace < 1) return -2;

	// Divide search space depending on process.
	part = maxSpace / nprocs;
	start = (spid - 1)*part;
	if (spid != nprocs) end = start + part - 1; 
	else end = maxSpace; 

	// Trial each combination of remaining bits of the key.
	// Write key to the processes buff if found.
	for (search_byte = start, counter = 0; search_byte < end; 
	     search_byte++, counter++) {
		trialkey_search(&trialkey, &key, &search_byte);
		EVP_CIPHER_CTX en, de;
		if (aes_init(trialkey, KEYLENGTH, &en, &de)) {
			fprintf(stderr, "Couldn't initialize AES cipher\n");
			return -1;
		}
		plaintext = (char *)aes_decrypt(&de, 
						(unsigned char *)cipher_in, 
                                                &cipher_length);
		EVP_CIPHER_CTX_cleanup(&en);
		EVP_CIPHER_CTX_cleanup(&de);
		if (strncmp(plaintext, plain_in, 28)); 
		else { 
			fprintf(stderr, 
				"\nP%d (PID#%d) found the key in %lu tries.\n",
				spid, (int)getppid(), counter);
			sprintf(buff, trialkey, KEYLENGTH+1);
			return 1;
		}
		free(plaintext);
	}
	return 0;
}
