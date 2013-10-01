#include "auth.h"
#include "rdp.h"

void mass_auth_entry(void *ptr) {
   MASS_AUTH_ARGS          *args;
   MASS_RDP                rdp;

   args = (MASS_AUTH_ARGS*)ptr;

   // open remote side tcp port for connections

   // loop
      // any new connections? if so accept them
         // flag connection as needing authentication first
      // any new data?
         // is sender authenticated OR trying to authenticate
            // bad:  handle authentication and/or rejection of connection
            // good: locate entity ID of account and ask on local side
            //       for server who own's that ID, alert client to process
            //       then put client on wait until we have a reply
            //          1. if reply found no entity then spawn new entity
            //          2. if reply found entity then alert client and it
            //          will take over the process from here, just route
            //          packets to the owning server of the client's entity
}