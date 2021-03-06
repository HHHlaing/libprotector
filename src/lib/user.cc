#include <vector>
#include <iterator>
#include <typeinfo>
#include <openssl/crypto.h>

#include <boost/algorithm/string.hpp>

#include "user.h"
#include "utils.h"

User::User(unsigned int keysize)
{
    this->keysize = keysize;
    this->numberG = BN_new();
    BIGNUM * number1 = BN_new();
    BN_one(number1);

    BN_add(this->numberG, number1, number1);
    
    BN_clear_free(number1);
};

User::~User(void)
{
    if (this->numberX_1)
        BN_clear_free(this->numberX_1);
    if (this->numberS)
        BN_clear_free(this->numberX_1);
    if (this->numberG)
        BN_clear_free(this->numberG);
    if (this->primeQ)
        BN_clear_free(this->primeQ);
}

void User::setUserKey(const BIGNUM * userKey)
{
    this->numberX_1 = BN_dup(userKey);
};

void User::setNumberG(const BIGNUM * serverKey)
{
    this->numberG = BN_dup(serverKey);
};

void User::setPrimeQ(const BIGNUM * Q)
{
    this->primeQ = BN_dup(Q);
};

void User::setSalt(const BIGNUM * saltKey)
{
    this->numberS = BN_dup(saltKey);
};

std::pair<BIGNUM *, BIGNUM *> User::UserTD(const unsigned char * component, unsigned int size_component)
{
    if(this->primeQ == NULL || this->numberG == NULL)
    {
        printf("Values not initialized\n");
        exit(-1);
    }
    /* Convert the user string into a number */
    BIGNUM * componentInBinary = BN_new();
    if (size_component > 160) // TODO: FIX THIS LIMIT ACCORDING TO THE KEYSIZE, SHOW A WARNING MESSAGE!
        size_component = 160;
    componentInBinary = BN_bin2bn(component, size_component, componentInBinary);

    BIGNUM * r = BN_new();
    BN_CTX * ctx = BN_CTX_new();
    BN_div(NULL, r, componentInBinary, this->primeQ, ctx);
    BN_CTX_free(ctx);

    BN_clear_free(componentInBinary);

    /* TODO: use a pseudorandom function */
    
    /* Calculate first part of the pair */
    BIGNUM * t_1 = BN_new();
    ctx = BN_CTX_new();    
    BN_mod_exp(t_1, this->numberG, r, this->primeQ, ctx);
    BN_CTX_free(ctx);

    /* Calculate second part of the pair */
    BIGNUM * t_2 = BN_new();
    ctx = BN_CTX_new();
    BN_mod_exp(t_2, t_1, this->numberX_1, this->primeQ, ctx);
    BN_CTX_free(ctx);

    BN_clear_free(r);

    /* Return the key pair */

    return std::make_pair(t_1, t_2);
};

char * User::FullUserTD(const std::vector<std::pair<BIGNUM *, BIGNUM *> > * user_content_names)
{

    char * new_content_name;
    unsigned int offset = 0;
    new_content_name = (char *) calloc(sizeof(char), user_content_names->size() * ((SECURITY_KEYSIZE/4)+1 + 1) * 2); // 1 for the / 1 for the \0

    std::vector<std::pair<BIGNUM *, BIGNUM *> >::const_iterator it;

    /* Initialize name */
    memcpy((void *) (new_content_name + offset), (void *) "/", 1 + 1);
    offset +=1;

    for (it = user_content_names->begin() ; it != user_content_names->end(); it++)
    {
        std::pair<BIGNUM *, BIGNUM *> pair_name = *it;
        //res->push_back(encrypted_bignum);

        //TODO: me puedo estar olvidando un cero aca
        char * aux_component = BN_bn2hex(pair_name.first);
        unsigned int size_aux_component = strnlen(aux_component, SECURITY_KEYSIZE/4 + 1 + 1); // Potentil \0 and potential minus sign
        memcpy((void *) (new_content_name + offset), (void *) aux_component, size_aux_component);
        offset += size_aux_component;
        memcpy((void *) (new_content_name + offset), (void *) "/", 1 + 1);
        offset +=1;
        OPENSSL_free(aux_component);

        aux_component = BN_bn2hex(pair_name.second);
        size_aux_component = strnlen(aux_component, SECURITY_KEYSIZE/4 + 1 + 1); // Potentil \0 and potential minus sign
        memcpy((void *) (new_content_name + offset), (void *) aux_component, size_aux_component);
        offset +=(size_aux_component);
        memcpy((void *) (new_content_name + offset), (void *) "/", 1 + 1);
        offset +=1;

        OPENSSL_free(aux_component);
    }

    return new_content_name;
}

// TODO: untested
std::pair<BIGNUM *, BIGNUM *> User::ContentProviderEnc(const unsigned char * component, unsigned int size_component)
{
    if(this->primeQ == NULL || this->numberG == NULL)
    {
        printf("Values not initialized\n");
        exit(-1);
    }
    
	
    /* Convert the user string into a number */
    BIGNUM * componentInBinary = BN_new();
    if (size_component > 160)
        size_component = 160;
    componentInBinary = BN_bin2bn(component, size_component, componentInBinary);
	
	// Calculate random r
	BIGNUM * r = BN_new();
    BN_rand(r, this->keysize, -1, 0);
	
	// Calculate g^r
	BIGNUM * t_1 = BN_new();
	BN_CTX * ctx = BN_CTX_new();    
    BN_mod_exp(t_1, this->numberG, r, this->primeQ, ctx);
    BN_CTX_free(ctx);
	
	// Calculate (g^r)^Xuser
	BIGNUM * aux = BN_new();
	ctx = BN_CTX_new();
	BN_mod_exp(aux, t_1, this->numberX_1, this->primeQ, ctx);
	BN_CTX_free(ctx);
	
	// Multiply the result
	BIGNUM * t_2 = BN_new();
	ctx = BN_CTX_new();
	BN_mod_mul(t_2, aux, componentInBinary, this->primeQ, ctx);
	BN_CTX_free(ctx);
	
	BN_clear_free(aux);
	BN_clear_free(r);
	BN_clear_free(componentInBinary);
	
	// Create pair (g^r, g^{r \times Xserver} \times d)
	return std::make_pair(t_1, t_2);
};

BIGNUM * User::UserDec(const std::pair<BIGNUM *, BIGNUM *> e)
{
	
	if(this->primeQ == NULL || this->numberG == NULL || this->numberX_1 == NULL)
    {
        printf("Values not initialized\n");
        exit(-1);
    }
	
	BIGNUM * aux = BN_new();
	BN_CTX * ctx = BN_CTX_new();
	BN_mod_exp(aux, e.first, this->numberX_1, this->primeQ, ctx);
	BN_CTX_free(ctx);
	
	ctx = BN_CTX_new();
	BIGNUM * aux2 = BN_mod_inverse(NULL, aux, this->primeQ, ctx);
	BN_CTX_free(ctx);
	
	BIGNUM * res = BN_new();
	ctx = BN_CTX_new();
	BN_mod_mul(res,  aux2, e.second, this->primeQ, ctx);
	BN_CTX_free(ctx);
	
	BN_clear_free(aux);
	BN_clear_free(aux2);
	
	return res;
}

/* WRAPPER FOR C */
extern "C" char * EncryptUserName(const char * original_content_name)
{
    User *u = new User(512); //TODO: change

    char * res_getUserK = retrieveKeyFromServer("userK:0");
    char * res_getPrimeQ = retrieveKeyFromServer("primeQ");
    
    BIGNUM * userKey = BN_new();
    BIGNUM * primeQ = BN_new();

    BN_hex2bn(&primeQ, res_getPrimeQ);
    BN_hex2bn(&userKey, res_getUserK);
    u->setUserKey(userKey);
    u->setPrimeQ(primeQ);

    BN_clear_free(primeQ);
    BN_clear_free(userKey);

    free(res_getPrimeQ);
    free(res_getUserK);

    /* split in keywords */
    std::vector<std::string> keywords;
    boost::split(keywords, original_content_name, boost::is_any_of("/"));

    std::vector<std::pair<BIGNUM *, BIGNUM *> > * vec = new std::vector<std::pair<BIGNUM *, BIGNUM *> >();
    for(std::vector<std::string>::iterator it = keywords.begin(); it != keywords.end(); it++)
    {
        std::string t = *it;
        if (t.compare("") == 0)
            continue;
        std::pair<unsigned char *, unsigned int> hashed_component = hash_message((const char*)(t.c_str()));

        std::pair<BIGNUM *, BIGNUM *> utrapdoor = u->UserTD(hashed_component.first, hashed_component.second);

        vec->push_back(utrapdoor);
        
        free(hashed_component.first);        
    }

    char * new_user_content_names = u->FullUserTD(vec);
    //printf("%s\n", new_user_content_names);

    // Atach ccnx: at the begining
    int length = strnlen(new_user_content_names, 1 + ((SECURITY_KEYSIZE/4 + 1)* vec->size() * 2) + 1 + 1);
    char *str2 = (char*) calloc(sizeof(char), length + 5 + 1);
    strcpy(str2, "ccnx:");
    strcat(str2, new_user_content_names);

    for(std::vector<std::pair<BIGNUM *, BIGNUM *> >::iterator it2 = vec->begin(); it2 != vec->end(); it2++)
    {
        std::pair<BIGNUM *, BIGNUM *> p = *it2;
        
        BN_clear_free(p.first);
        BN_clear_free(p.second);


    }
    delete vec;

    delete u;

    

    free(new_user_content_names);

    return str2;
}

// Encode and encrypt the content
extern "C" char * EncryptUserContentNoNetwork(const unsigned char * original_content, const unsigned int content_len, const BIGNUM * primeQ, const BIGNUM * userKey)
{
	//std::pair<unsigned char *, unsigned int> hashed_component = encodeIntoBase64(original_content, content_len);
	User *u = new User(512); // TODO: CHANGE

    u->setPrimeQ(primeQ);
    u->setUserKey(userKey);
	std::pair<BIGNUM *, BIGNUM *> p1 = u->ContentProviderEnc(original_content, content_len);
		
	char * first_part_encr = BN_bn2hex(p1.first);
	char * second_part_encr = BN_bn2hex(p1.second);
	
	/* pack both in a string: append both parts with a pipe */
    char *encrypted_content = (char*) calloc(sizeof(char), (SECURITY_KEYSIZE/4 + 1) * 2 + 1 + 1);
    strcpy(encrypted_content, first_part_encr);
	strcat(encrypted_content, "|");
    strcat(encrypted_content, second_part_encr);
	
	/* Free all the elements */
	BN_clear_free(p1.first);
	BN_clear_free(p1.second);
	OPENSSL_free(first_part_encr);
	OPENSSL_free(second_part_encr);
	
	return encrypted_content;
}

extern "C" char * libprotector_EncryptUserContentWithKeys(const unsigned char * original_content, const unsigned int content_len, const char * user_key, const char * prime_q)
{
    BIGNUM * primeQ = BN_new();
    BIGNUM * userKey = BN_new();
	
    BN_hex2bn(&primeQ, prime_q);
    BN_hex2bn(&userKey, user_key);

    User *u = new User(512); // TODO: CHANGE

    u->setPrimeQ(primeQ);
    u->setUserKey(userKey);
	std::pair<BIGNUM *, BIGNUM *> p1 = u->ContentProviderEnc(original_content, content_len);
	delete u;
	
	char * first_part_encr = BN_bn2hex(p1.first);
	char * second_part_encr = BN_bn2hex(p1.second);
	
	/* pack both in a string: append both parts with a pipe */
    char *encrypted_content = (char*) calloc(sizeof(char), (SECURITY_KEYSIZE/4 + 1) * 2 + 1 + 1);
    strcpy(encrypted_content, first_part_encr);
	strcat(encrypted_content, "|");
    strcat(encrypted_content, second_part_encr);
	
	/* Free all the elements */
	BN_clear_free(p1.first);
	BN_clear_free(p1.second);
	OPENSSL_free(first_part_encr);
	OPENSSL_free(second_part_encr);
	
	BN_clear_free(primeQ);
	BN_clear_free(userKey);
	
	return encrypted_content;
}

// Encode and encrypt the content
extern "C" char * libprotector_EncryptUserContent(const unsigned char * original_content, const unsigned int content_len)
{
	//std::pair<unsigned char *, unsigned int> hashed_component = encodeIntoBase64(original_content, content_len);
	
	char * res_getUserK = retrieveKeyFromServer("userK:0");
    char * res_getPrimeQ = retrieveKeyFromServer("primeQ");
	
	char * encrypted_content = libprotector_EncryptUserContentWithKeys(original_content, content_len, res_getUserK, res_getPrimeQ);
	
	free(res_getPrimeQ);
	free(res_getUserK);
	
	return encrypted_content;
}

extern "C" unsigned char * libprotector_ReDecryptContentWithKeys(const char * encrypted_content, const char * res_getUserK, const char * res_getPrimeQ)
{
    BIGNUM * primeQ = BN_new();
    BIGNUM * userKey = BN_new();
	
    BN_hex2bn(&primeQ, res_getPrimeQ);
    BN_hex2bn(&userKey, res_getUserK);

    User *u = new User(512); // TODO: CHANGE

    u->setPrimeQ(primeQ);
    u->setUserKey(userKey);
	
	// Convert char into BIGNUM 
	std::vector<std::string> keywords;
    boost::split(keywords, encrypted_content, boost::is_any_of("|"));

	BIGNUM *b1 = BN_new();
	BIGNUM *b2 = BN_new();

	const char * c1 = keywords[0].c_str();
	const char * c2 = keywords[1].c_str();
	BN_hex2bn(&b1, c1);
	BN_hex2bn(&b2, c2);

	std::pair<BIGNUM *, BIGNUM *> utrapdoor = std::make_pair(b1, b2);
	
	BIGNUM * res = u->UserDec(utrapdoor);
	char * res_char = (char *) calloc(sizeof(char), SECURITY_KEYSIZE/4 + 2);
	BN_bn2bin(res, (unsigned char *) res_char);
	
	delete u;
	
	BN_clear_free(res);
	
	BN_clear_free(b1);
	BN_clear_free(b2);
	
	BN_clear_free(userKey);
	BN_clear_free(primeQ);
	
	return res_char;
}

extern "C" unsigned char * libprotector_ReDecryptContent(const char * encrypted_content)
{
	char * res_getUserK = retrieveKeyFromServer("userK:0");
    char * res_getPrimeQ = retrieveKeyFromServer("primeQ");
	
	char * res_char = libprotector_ReDecryptContentWithKeys(encrypted_content, res_getUserK, res_getPrimeQ);
	
	free(res_getPrimeQ);
	free(res_getUserK);
	
	return (unsigned char *) res_char;//res_final;
}

extern "C" unsigned char * libprotector_ReDecryptAndSplitContent(const char * encrypted_content)
{
	char * res_getUserK = retrieveKeyFromServer("userK:0");
    char * res_getPrimeQ = retrieveKeyFromServer("primeQ");
	
	BIGNUM * primeQ = BN_new();
    BIGNUM * userKey = BN_new();
	
    BN_hex2bn(&primeQ, res_getPrimeQ);
    BN_hex2bn(&userKey, res_getUserK);

    User *u = new User(512); // TODO: CHANGE

    u->setPrimeQ(primeQ);
    u->setUserKey(userKey);
	
	// Convert char into BIGNUM 
	std::vector<std::string> keywords;
    boost::split(keywords, encrypted_content, boost::is_any_of("|"));

    char * b64_message = (char *) calloc(sizeof(char), (SECURITY_KEYSIZE/4 + 2)*keywords.size()/2);
    b64_message[0] = '\0';
    int offset = 0;

    //printf("There are # %d keywords (%d)\n", keywords.size(), strlen(encrypted_content));

    for(unsigned int i=0; i<keywords.size(); i+=2)
    {
	    BIGNUM *b1 = BN_new();
	    BIGNUM *b2 = BN_new();

	    const char * c1 = keywords[i].c_str();
	    const char * c2 = keywords[i+1].c_str();
	    BN_hex2bn(&b1, c1);
	    BN_hex2bn(&b2, c2);

	    std::pair<BIGNUM *, BIGNUM *> utrapdoor = std::make_pair(b1, b2);
	
	    BIGNUM * res = u->UserDec(utrapdoor);
        //TODO: HERE IS THE PROBLEM!!!
	    char * aux = (char *) calloc(sizeof(char), SECURITY_KEYSIZE/4 + 2);
	    int copied_bytes = BN_bn2bin(res, (unsigned char *) aux);
	    BN_clear_free(res);
        memcpy(b64_message+offset, aux, copied_bytes);
        //printf("COPIED: %d\n", copied_bytes);
        offset += copied_bytes;
        
        free(aux);
        
        BN_clear_free(b1);
        BN_clear_free(b2);
    }
    b64_message[offset+1] = '\0';
    
    delete u;
    
    BN_clear_free(primeQ);
    BN_clear_free(userKey);
    
    free(res_getPrimeQ);
    free(res_getUserK);
    

    //printf("The base64 message is %s (%d vs %d)\n", b64_message, strlen(b64_message),  offset);
	
	return (unsigned char*) b64_message;
}

/* libprotector C interface */
extern "C" User * libprotector_User_new(unsigned int keysize)
{
    return new User(keysize);
}

extern "C" void libprotector_User_setUserKey(User * u, const char * user_key)
{
    BIGNUM * aux = BN_new();
    BN_hex2bn(&aux, user_key);
    reinterpret_cast<User *>(u)->setUserKey(aux);
    BN_clear_free(aux);
};

extern "C" void libprotector_User_setNumberG(User * u, const char * number_g)
{
    BIGNUM * aux = BN_new();
    BN_hex2bn(&aux, number_g);
    reinterpret_cast<User *>(u)->setNumberG(aux);
    BN_clear_free(aux);
};

extern "C" void libprotector_User_setPrimeQ(User * u, const char * prime_q)
{
    BIGNUM * aux = BN_new();
    BN_hex2bn(&aux, prime_q);
    reinterpret_cast<User *>(u)->setPrimeQ(aux);
    BN_clear_free(aux);
};

extern "C" void libprotector_User_setSalt(User * u, const char * salt_key)
{
    BIGNUM * aux = BN_new();
    BN_hex2bn(&aux, salt_key);
    reinterpret_cast<User *>(u)->setSalt(aux);
    BN_clear_free(aux);
};

extern "C" void libprotector_User_UserTD(User * u, const unsigned char * component, unsigned int size_component, char *& p1, char *& p2)
{
    std::pair<BIGNUM *, BIGNUM *> t = reinterpret_cast<User *>(u)->UserTD(component, size_component);
    unsigned int keysize = reinterpret_cast<User *>(u)->keysize;
    p1 = (char *) calloc(sizeof(char), ((keysize/4)) );
    char * aux_component = BN_bn2hex(t.first);
    memcpy((void *) (p1), (void *) aux_component, (keysize/4));
    
    p2 = (char *) calloc(sizeof(char), ((keysize/4)) );
    char * aux_component2 = BN_bn2hex(t.second);
    memcpy((void *) (p2), (void *) aux_component2, (keysize/4));
    
    free(aux_component);
    free(aux_component2);
}

extern "C" void libprotector_User_ContentTD(User * u, const unsigned char * block, const unsigned int block_size, char *& p1, char *& p2)
{
    std::pair<BIGNUM *, BIGNUM *> t = reinterpret_cast<User *>(u)->ContentProviderEnc(block, block_size);
    
    
    unsigned int keysize = reinterpret_cast<User *>(u)->keysize;
    p1 = (char *) calloc(sizeof(char), ((keysize/4)) );
    char * aux_component = BN_bn2hex(t.first);
    memcpy((void *) (p1), (void *) aux_component, (keysize/4));
    
    p2 = (char *) calloc(sizeof(char), ((keysize/4)) );
    char * aux_component2 = BN_bn2hex(t.second);
    memcpy((void *) (p2), (void *) aux_component2, (keysize/4));
    
    free(aux_component);
    free(aux_component2);
}

extern "C" void libprotector_User_ClientDec(User * u, const char * component1, const char * component2, char *& p1)
{
    unsigned int keysize = reinterpret_cast<User *>(u)->keysize;
    
    BIGNUM * aux = BN_new();
    BN_hex2bn(&aux, component1);
    
    BIGNUM * aux2 = BN_new();
    BN_hex2bn(&aux2, component2);
    
    std::pair<BIGNUM *, BIGNUM *> encrypted_payload = std::make_pair(aux, aux2);
    
    BIGNUM * decrypted_value = reinterpret_cast<User *>(u)->UserDec(encrypted_payload);
    
    p1 = (char *) calloc(sizeof(char), ((keysize/4)) );
    char * aux_component = BN_bn2hex(decrypted_value);
    
    BN_bn2bin(decrypted_value, (unsigned char *) p1);
	
    free(aux_component);
}




