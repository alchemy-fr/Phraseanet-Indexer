/*
 * File:   stemmer_grp.h
 * Author: gaulier
 *
 * Created on 3 janvier 2013, 18:27
 */

#ifndef STEMMER_GRP_H
#define	STEMMER_GRP_H

#ifdef	__cplusplus
extern "C"
{
#endif

	class CSTEMMER_GRP
	{
		struct x {
			void *p;
			const char *name[8];
			int n;
			const char *ln;
		};

		private:
			struct x m[50];
			int n;
			unsigned int ml;

		public:
			CSTEMMER_GRP()
			{
				this->n  = 0;
				this->ml = 0;
			}

			void add_module(void *p, const char *name)
			{
				int i;
				for(i=0; i<this->n; i++)
				{
					if(this->m[i].p == p)
						break;
				}
				if(i == this->n && i < 50)
				{
					this->m[i].p = p;
					this->m[i].n = 0;
					this->m[i].ln = NULL;
					this->n++;
				}
				if(i<50)
				{
					if(this->m[i].n < 8)
					{
						unsigned int l=0;
						this->m[i].name[this->m[i].n] = name;
						if(this->m[i].ln == NULL || (l=strlen(name))>strlen(this->m[i].ln))
						{
							this->m[i].ln = name;
							if(l > this->ml)
								this->ml = l;
						}
						this->m[i].n++;
					}
				}
			}

			void dump()
			{
				printf("/----- stemmers --------------\n");
				int i, j;
				char fmt[50];
				sprintf(fmt, "| %%-%ds : ", this->ml);
				for(i=0; i<this->n; i++)
				{
					printf(fmt, this->m[i].ln);
					for(j=0; j<this->m[i].n; j++)
					{
						if(strlen(this->m[i].name[j]) <= 4)
							printf("%-4s ", this->m[i].name[j]);
					}
					printf("\n");
				}
				printf("\\-----------------------------\n");
			}
	};


#ifdef	__cplusplus
}
#endif

#endif	/* STEMMER_GRP_H */

