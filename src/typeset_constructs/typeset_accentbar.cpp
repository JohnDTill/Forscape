# i n c l u d e   " t y p e s e t _ a c c e n t b a r . h " 
 
 # i n c l u d e   " t y p e s e t _ s u b p h r a s e . h " 
 
 n a m e s p a c e   H o p e   { 
 
 n a m e s p a c e   T y p e s e t   { 
 
 A c c e n t B a r : : A c c e n t B a r ( ) { 
         s e t u p U n a r y A r g ( ) ; 
 } 
 
 c h a r   A c c e n t B a r : : c o n s t r u c t C o d e ( )   c o n s t   n o e x c e p t   {   r e t u r n   A C C E N T B A R ;   } 
 
 # i f n d e f   H O P E _ T Y P E S E T _ H E A D L E S S 
 v o i d   A c c e n t B a r : : u p d a t e S i z e S p e c i f i c ( )   n o e x c e p t   { 
         w i d t h   =   c h i l d ( ) - > w i d t h ; 
         u n d e r _ c e n t e r   =   c h i l d ( ) - > u n d e r _ c e n t e r ; 
         s h o u l d _ d r o p   =   h a s S m a l l C h i l d ( ) ; 
         a b o v e _ c e n t e r   =   c h i l d ( ) - > a b o v e _ c e n t e r   +   ! s h o u l d _ d r o p   *   v o f f s e t ; 
 } 
 
 v o i d   A c c e n t B a r : : u p d a t e C h i l d P o s i t i o n s ( )   { 
         c h i l d ( ) - > x   =   x ; 
         c h i l d ( ) - > y   =   y   +   ! s h o u l d _ d r o p * v o f f s e t ; 
 } 
 
 v o i d   A c c e n t B a r : : p a i n t S p e c i f i c ( P a i n t e r &   p a i n t e r )   c o n s t   { 
         p a i n t e r . d r a w L i n e ( x + h o f f s e t ,   y + s h o u l d _ d r o p * d r o p ,   w i d t h ,   0 ) ; 
 } 
 # e n d i f 
 
 } 
 
 } 
 