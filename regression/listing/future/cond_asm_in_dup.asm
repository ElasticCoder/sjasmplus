;dup uses string model while conditional expression uses file model
        device zxspectrum128

        org #8000
	if 1
	db 'hello'
	endif

	dup 8
	ldi:ldi

	if 1
	dec de,de
	endif

	ldi:ldi
	edup
